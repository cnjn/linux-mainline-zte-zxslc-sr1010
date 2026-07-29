# 0x13898 idm_get_smct_all_trap

## Status

- Status: complete
- Confidence: verified pointer validation, rate-limited error path, volatile
  bit extraction, status returns, and exported-interface status.
- Size: `0x60` bytes, 23 ARM64 instructions.
- Recovered signature: `int idm_get_smct_all_trap(u32 *mode)`.

## Semantics

For a non-null output pointer, reads IDM base bit 14, writes zero or one to
`*mode`, and returns zero. A null pointer returns `-1`; it calls
`__printk_ratelimit("idm_get_smct_all_trap")` and emits
`idm_get_smct_all_trap mode NULL` only when permitted. No lock or barrier is
used around the register read.

## Caller Context

There are no direct module code callers. `__ksymtab_idm_get_smct_all_trap @
0x1c774` exports the getter, complementing `idm_set_smct_all_trap`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
