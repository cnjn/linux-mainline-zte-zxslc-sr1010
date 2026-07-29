# 0x130fc idm_tx_debug_set

## Status

- Status: complete
- Confidence: verified 32-bit store and exported-interface status.
- Size: `0xc` bytes, 3 ARM64 instructions.
- Recovered signature: `void idm_tx_debug_set(s32 value)`.

## Semantics

Stores `value` directly to `net_tx_debug`. The unchanged `W0` register at
return has no evidenced semantic return role.

## Caller Context

No direct module code caller exists; `__ksymtab_idm_tx_debug_set @ 0x1c834`
exports the setter. `net_tx_debug` gates TX diagnostics in CPU and IDM TX paths.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
