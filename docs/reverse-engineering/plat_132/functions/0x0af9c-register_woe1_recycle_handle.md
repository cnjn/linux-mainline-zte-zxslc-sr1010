# 0x0af9c register_woe1_recycle_handle

## Status

- Status: complete
- Confidence: verified slot-1 store, residual return, export, and recycle
  dispatch ABI/consumer; external callback behavior remains unknown.
- Size: `0x0c` bytes, 3 ARM64 instructions.
- Recovered signature:
  `zte_recycle_callback_t register_woe1_recycle_handle(zte_recycle_callback_t callback)`.

## Semantics

Stores the recycle callback in `idm_recycle_cb[1]` and returns it unchanged. Null
is accepted. It has no validation, synchronization, reference/lifetime
management, or previous-slot return.

## Caller Context

There are no direct in-module callers. The API is exported and is the slot-1
sibling of `register_woe_recycle_handle` and `register_woe2_recycle_handle`.
`net_check_reorder_rls_nolock @ 0x0b050` invokes a non-null slot-1 callback as
`callback(1, &context)` for a nonzero clamped release count.

## Concurrency and Ownership

- Publication is unsynchronized with dispatch.
- Direct consumers hold `idm_lock_tx`, while the provider retains callback
  ownership and lifetime responsibility.

## Evidence

- Complete three-instruction ARM64 body at `0xaf9c` through `0xafa4`.
- Store target `0x27938` is exactly `idm_recycle_cb + 8`.
- No direct IDA callers and runtime kallsyms export.
- Reorder-release dispatcher at `0xb050` resolves slot-1 callback ABI and use.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Callback provider/lifecycle and slot-1 completion-ring processing behavior.
