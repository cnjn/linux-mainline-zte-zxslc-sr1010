# 0x130f0 idm_rx_test

## Status

- Status: complete
- Confidence: verified constant return and exported-interface status.
- Size: `0x8` bytes, 2 ARM64 instructions.
- Recovered signature: `int idm_rx_test(void)`.

## Semantics

Executes `MOV W0,#0` and returns. It has no input, global, MMIO, allocation, or
locking behavior.

## Caller Context

There are no direct module code callers. `__ksymtab_idm_rx_test @ 0x1c7e0`
exports the entry for external consumers.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
