# 0x11f60 greg_rgmii_intf_mode_set

## Status

- Status: complete
- Confidence: verified mode predicate and RMW; semantic void ABI.
- Size: `0x24` bytes, 9 ARM64 instructions.
- Recovered signature: `void greg_rgmii_intf_mode_set(u8 mode)`.

## Semantics

Clears NPPT `+0x30` bits 17 and 18. Exact mode zero then sets both; all other
low-byte modes leave them clear. The current direct caller passes one.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
