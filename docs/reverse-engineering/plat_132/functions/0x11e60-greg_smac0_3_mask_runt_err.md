# 0x11e60 greg_smac0_3_mask_runt_err

## Status

- Status: complete
- Confidence: verified unchecked offset arithmetic and one RMW.
- Size: `0x20` bytes, 8 ARM64 instructions.
- Recovered signature: `void greg_smac0_3_mask_runt_err(u32 mac)`.

## Semantics

Computes `4 * ((mac + 13) & 0x3fffffff)` and ORs bit 18 into that NPPT word. It
performs no range check; the all-SMAC wrapper supplies values zero through three,
yielding offsets `0x34` through `0x40`.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
