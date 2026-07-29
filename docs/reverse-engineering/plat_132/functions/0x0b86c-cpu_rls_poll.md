# 0x0b86c cpu_rls_poll

## Status

- Status: complete
- Confidence: verified lock acquisition/release instructions, reorder callback,
  NAPI slot/source mapping, raw counter, and constant return.
- Size: `0x64` bytes, 24 ARM64 instructions.
- Recovered signature: `int cpu_rls_poll(struct napi_struct *napi, int budget)`.

## Semantics

This source-3 NAPI callback ignores both arguments. It increments raw counter
`0x28124`, acquires `idm_lock_tx` through the module-local
`do_raw_spin_lock`, calls `net_check_reorder_rls_nolock`, then releases the
lock with an inline `STLRB wzr, [idm_lock_tx]` store-release to the lock's low
byte.

It unconditionally calls `napi_complete(int_info + 0x4e0)`, invokes
`idm_int_enable` through `cpu_net_ops + 0x8` with `cpu_net_info_word_c`, and
returns constant `1`. It has no budget exhaustion path and performs no RX,
GRO, allocation, or MMIO operation itself.

## Relationship to IRQ Routing

`idm_rls_int` masks `idm_info.word_c`, then calls `cpu_net_int(3)`. Slot 3 is
registered with this callback, so the poll re-enables precisely the source it
was scheduled to process.

## Locking

`do_raw_spin_lock @ 0xb768` uses an `LDAXR`/`STXR` acquire loop and falls back
to `queued_spin_lock_slowpath` if the lock is held. The explicit `STLRB` in the
caller is a release store, not an ordinary C assignment. The lock covers the
full `net_check_reorder_rls_nolock` call.

## Evidence

- Complete 24-instruction ARM64 disassembly at `0xb86c` through `0xb8cc`.
- Full local lock helper disassembly at `0xb768` through `0xb7a0`.
- Direct `net_check_reorder_rls_nolock @ 0xb050` reconstruction.
- `cpu_net_init @ 0xe220` registers the callback at slot 3.
- `idm_rls_int @ 0x13b60` supplies source index 3 after masking word c.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Exact reorder-release ring ownership and callback registration are pending
  `net_check_reorder_rls_nolock` and companion-module reconstruction.
- The consumer of raw counter `0x28124` is unknown.
