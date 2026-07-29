# 0x13194 set_last_jumbo_cnt

## Status

- Status: complete
- Confidence: verified 32-bit global store and exported-interface status.
- Size: `0x10` bytes, 3 ARM64 instructions.
- Recovered signature: `void set_last_jumbo_cnt(u32 value)`.

## Semantics

Stores `value` to `last_jumbo_cnt` with no validation, synchronization, or
other side effect.

## Caller Context

No direct module code caller exists; `__ksymtab_set_last_jumbo_cnt @ 0x1cd14`
exports the setter.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
