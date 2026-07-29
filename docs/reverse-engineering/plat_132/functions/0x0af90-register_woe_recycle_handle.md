# 0x0af90 register_woe_recycle_handle

## Status

- Status: complete
- Confidence: verified slot-0 store, residual return, export, and recycle
  dispatch ABI/consumer; external callback behavior remains unknown.
- Size: `0x0c` bytes, 3 ARM64 instructions.
- Recovered signature:
  `zte_recycle_callback_t register_woe_recycle_handle(zte_recycle_callback_t callback)`.

## Semantics

Stores the recycle callback in `idm_recycle_cb[0]` and returns that same pointer.
It accepts null, overwrites any previous value, and has no validation,
synchronization, reference/lifetime management, or previous-slot return.

## Caller Context

There are no direct in-module callers. The API is exported and has two adjacent
siblings for slots 1 and 2. `net_check_reorder_rls_nolock @ 0x0b050` invokes a
non-null slot-0 callback as `callback(0, &context)` when its clamped release
count is nonzero. The observed context contains a completion-ring base, ring
size, current release index, and count.

## Concurrency and Ownership

- Publication is unsynchronized with dispatch.
- Direct consumers hold `idm_lock_tx`, but the provider owns callback lifetime
  and synchronization with registration.

## Evidence

- Complete three-instruction ARM64 body at `0xaf90` through `0xaf98`.
- No direct IDA callers; runtime kallsyms export.
- Reorder-release dispatcher at `0xb050` resolves slot-0 callback ABI and use.
- Adjacent setters at `0xaf9c` and `0xafa8` establish the three-slot family.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Callback provider, lifecycle, and semantics for completion-ring data.
