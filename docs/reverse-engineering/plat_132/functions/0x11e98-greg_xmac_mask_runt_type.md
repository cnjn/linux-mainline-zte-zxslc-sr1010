# 0x11e98 greg_xmac_mask_runt_type

## Status

- Status: complete
- Confidence: verified unchecked offset arithmetic and one RMW.
- Size: `0x24` bytes, 9 ARM64 instructions.
- Recovered signature: `void greg_xmac_mask_runt_type(u32 xmac)`.

## Semantics

Computes `4 * ((xmac + 0x34) & 0x3fffffff)` and ORs `0x410` into that NPPT
word. The all-SMAC wrapper supplies zero and one, producing offsets `0xd0` and
`0xd4`.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
