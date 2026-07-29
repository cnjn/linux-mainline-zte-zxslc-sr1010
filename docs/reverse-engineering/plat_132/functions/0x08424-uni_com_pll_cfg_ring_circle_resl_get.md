# 0x08424 uni_com_pll_cfg_ring_circle_resl_get

## Status

- Status: complete
- Confidence: verified volatile read, four-bit extraction, returned value,
  vendor log, absent internal xrefs, and exported ABI.
- Size: `0x3c` bytes, 14 ARM64 instructions.
- Recovered signature: `uint32_t uni_com_pll_cfg_ring_circle_resl_get(void)`.

## Semantics

Reads `uni_serdes_base + 0x04`, extracts bits 23-26, logs the field, and
returns the unsigned four-bit result. It has no internal IDB xrefs and is
exported through `__ksymtab_uni_com_pll_cfg_ring_circle_resl_get`.

## Evidence

- Complete ARM64 body at `0x8424` through `0x845c`.
- Volatile load at `0x8438` and `UBFX #23,#4` extraction at `0x8444`.
- Value log at `0x844c`; returned field at `0x8450`.
- IDA type at `0x8424` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
