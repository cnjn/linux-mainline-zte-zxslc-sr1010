# 0x13120 idm_wifi_rx_debug_set

## Status

- Status: complete
- Confidence: verified both 32-bit stores and exported-interface status.
- Size: `0x14` bytes, 5 ARM64 instructions.
- Recovered signature: `void idm_wifi_rx_debug_set(s32 value)`.

## Semantics

Copies the same `value` to `idm_rx_debug` and then `np1_trap_debug`. Neither
store is conditional or synchronized.

## Caller Context

No direct module code caller exists; `__ksymtab_idm_wifi_rx_debug_set @
0x1c84c` exports the setter. Both targets gate Wi-Fi and NP1 trap diagnostics.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
