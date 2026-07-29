# 0x131f0 set_last_buffer_idx

## Status

- Status: complete
- Confidence: verified selector branches, all three 32-bit stores, invalid
  selector no-op, and exported-interface status.
- Size: `0x44` bytes, 17 ARM64 instructions.
- Recovered signature: `void set_last_buffer_idx(u32 pool, u32 value)`.

## Semantics

Stores `value` to `last_normal_idx`, `last_jumbo_idx`, or `last_extral_idx`
when `pool` is respectively zero, one, or two. Any other selector is a silent
no-op. There is no lock, barrier, or range check.

## Caller Context

No direct module code caller exists; `__ksymtab_set_last_buffer_idx @ 0x1ccfc`
exports the setter API.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
