# 0x0afa8 register_woe2_recycle_handle

## Status

- Status: complete
- Confidence: verified slot-2 store, residual return, export, and recycle
  dispatch ABI/consumer; external callback behavior remains unknown.
- Size: `0x0c` bytes, 3 ARM64 instructions.
- Recovered signature:
  `zte_recycle_callback_t register_woe2_recycle_handle(zte_recycle_callback_t callback)`.

## Semantics

Stores the recycle callback in `idm_recycle_cb[2]` and returns it unchanged. Null
is accepted. It has no validation, synchronization, reference/lifetime
management, or previous-slot return.

## Caller Context

There are no direct in-module callers. The API is exported and completes the
three-slot family with `register_woe_recycle_handle` and
`register_woe1_recycle_handle`. `net_check_reorder_rls_nolock @ 0x0b050` invokes
a non-null slot-2 callback as `callback(2, &context)` for a nonzero clamped
release count when `rls_ring_num_max` reaches three. `cpu_net_init` does that
only for CPU types 133 or 129.

## Concurrency and Ownership

- Publication is unsynchronized with dispatch.
- Direct consumers hold `idm_lock_tx`, while the provider retains callback
  ownership and lifetime responsibility.

## Evidence

- Complete three-instruction ARM64 body at `0xafa8` through `0xafb0`.
- Store target `0x27940` is exactly `idm_recycle_cb + 0x10`.
- No direct IDA callers; runtime kallsyms exports the function.
- Reorder-release dispatcher at `0xb050` resolves slot-2 callback ABI and use.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Callback provider/lifecycle and slot-2 completion-ring processing behavior.
