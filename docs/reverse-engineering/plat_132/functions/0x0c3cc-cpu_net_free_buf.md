# 0x0c3cc cpu_net_free_buf

## Status

- Status: complete
- Confidence: verified register forwarding, indirect ops-table slot, sole
  caller, and semantically unused residual return register.
- Size: `0x20` bytes, 8 ARM64 instructions.
- Recovered signature: `void cpu_net_free_buf(void *buffer, u32 pool)`.

## Semantics

Forwards the live first two ABI arguments unchanged to the function pointer at
`cpu_net_ops + 0x30`. It performs no null check on `cpu_net_ops` or the slot,
and it has no local allocation, buffer mutation, lock, MMIO, or ownership
policy. The CPU-net IDM ops table identifies that slot as the backend buffer-free
operation.

The indirect callee's raw return register is preserved by the wrapper, but the
sole in-module caller does not semantically consume it: its following
`__my_cpu_offset` call ignores its incoming argument and reads CPU-local state.
The semantic return type is therefore void.

## Caller Context

`idm_skb_stack_push @ 0xffd8` is the sole direct caller. When skb word `+0x114`
has both bits 16 and 0 set, it passes `skb + 0x128` as the buffer pointer and
zero as the pool, then returns the skb to its per-CPU FIFO path. Other flag
states use `__dev_kfree_skb_any` instead.

## Evidence

- Complete eight-instruction ARM64 body at `0xc3cc` through `0xc3e8`.
- `BLR` through the exact CPU-net ops offset `+0x30` without writes to `x0/x1`.
- Sole direct caller disassembly and the `__my_cpu_offset @ 0xfba0` body.
- IDM operations-table mapping established by `idm_init`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Full backend buffer-free ABI and ownership policy beyond the observed `(buffer,
  pool)` arguments.
- Why `idm_skb_stack_push` retains the indirect return register before calling
  the argument-ignoring CPU-local helper.
