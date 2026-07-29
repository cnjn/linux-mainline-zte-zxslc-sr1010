# 0x13174 set_last_extral_cnt

## Status

- Status: complete
- Confidence: verified 32-bit global store and exported-interface status.
- Size: `0x10` bytes, 3 ARM64 instructions.
- Recovered signature: `void set_last_extral_cnt(u32 value)`.

## Semantics

Stores `value` to `last_extral_cnt` with no validation, synchronization, or
other side effect. The vendor spelling `extral` is preserved.

## Caller Context

No direct module code caller exists; `__ksymtab_set_last_extral_cnt @ 0x1cd08`
exports the setter.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
