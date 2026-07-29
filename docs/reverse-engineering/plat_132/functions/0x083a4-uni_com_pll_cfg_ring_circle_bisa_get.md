# 0x083a4 uni_com_pll_cfg_ring_circle_bisa_get

## Status

- Status: complete
- Confidence: verified one volatile register read, four-bit extraction, both
  vendor logs, returned value, and exported ABI.
- Size: `0x48` bytes, 16 ARM64 instructions.
- Recovered signature: `uint32_t uni_com_pll_cfg_ring_circle_bisa_get(void)`.

## Semantics

Reads `uni_serdes_base + 0x04`, extracts bits 16-19, logs the resulting value,
prints the vendor ring-circle mapping, and returns the unsigned four-bit field.
The sole current data xref is export metadata for
`__ksymtab_uni_com_pll_cfg_ring_circle_bisa_get`.

## Evidence

- Complete ARM64 body at `0x83a4` through `0x83e8`.
- Volatile load at `0x83b8` and `UBFX #16,#4` extraction at `0x83c4`.
- Value log at `0x83cc`, mapping log at `0x83d8`, and returned field at
  `0x83dc`.
- IDA type at `0x83a4` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
