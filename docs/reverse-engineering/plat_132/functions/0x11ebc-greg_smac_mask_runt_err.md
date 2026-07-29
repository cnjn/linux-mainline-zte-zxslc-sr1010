# 0x11ebc greg_smac_mask_runt_err

## Status

- Status: complete
- Confidence: verified fixed call order and semantic void ABI.
- Size: `0x44` bytes, 17 ARM64 instructions.
- Recovered signature: `void greg_smac_mask_runt_err(void)`.

## Semantics

Calls the SMAC 0-3 mask helper with 0, 1, 2, 3; then SMAC6 mask; then XMAC mask
with 0 and 1. It has no direct MMIO of its own and ignores all child residual
values.

## Caller Context

Sole direct caller: `greg_init`.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
