# 0x11e80 greg_smac6_mask_runt_err

## Status

- Status: complete
- Confidence: verified.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `void greg_smac6_mask_runt_err(void)`.

## Semantics

ORs bit 16 into `nppt_base + 0x4c`. The all-SMAC wrapper is its only direct
caller; no lock or return contract exists.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
