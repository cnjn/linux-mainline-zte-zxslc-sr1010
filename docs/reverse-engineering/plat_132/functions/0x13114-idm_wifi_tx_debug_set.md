# 0x13114 idm_wifi_tx_debug_set

## Status

- Status: complete
- Confidence: verified 32-bit store and exported-interface status.
- Size: `0xc` bytes, 3 ARM64 instructions.
- Recovered signature: `void idm_wifi_tx_debug_set(s32 value)`.

## Semantics

Stores `value` directly to `idm_tx_debug`. It performs no validation,

## Caller Context

No direct module code caller exists; `__ksymtab_idm_wifi_tx_debug_set @
0x1c858` exports the setter. `idm_wifi_tx` consumes the debug budget.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
