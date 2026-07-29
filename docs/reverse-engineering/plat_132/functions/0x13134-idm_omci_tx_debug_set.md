# 0x13134 idm_omci_tx_debug_set

## Status

- Status: complete
- Confidence: verified 32-bit store and exported-interface status.
- Size: `0xc` bytes, 3 ARM64 instructions.
- Recovered signature: `void idm_omci_tx_debug_set(s32 value)`.

## Semantics

Stores `value` directly to `omci_tx_debug`, without validation or
synchronization.

## Caller Context

No direct module code caller exists; `__ksymtab_idm_omci_tx_debug_set @
0x1c7a4` exports the setter. `idm_omci_tx` consumes the target debug state.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
