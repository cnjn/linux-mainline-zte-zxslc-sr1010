# 0x13108 idm_rx_debug_set

## Status

- Status: complete
- Confidence: verified 32-bit store and exported-interface status.
- Size: `0xc` bytes, 3 ARM64 instructions.
- Recovered signature: `void idm_rx_debug_set(s32 value)`.

## Semantics

Stores `value` directly to `net_rx_debug`, without range checks, barriers, or
local synchronization. The preserved input register is not a semantic return.

## Caller Context

No direct module code caller exists; `__ksymtab_idm_rx_debug_set @ 0x1c7d4`
exports the setter. CPU RX/poll diagnostic paths consume `net_rx_debug`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
