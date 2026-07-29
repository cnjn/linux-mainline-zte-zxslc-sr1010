# 0x01870 an1_pll_en_get

## Status

- Status: complete
- Confidence: verified PLL offset/bit, log, unsigned return, and absence of
  direct code xrefs.
- Size: `0x3c` bytes, 14 ARM64 instructions.
- Recovered signature: `u32 an1_pll_en_get(void)`.

## Semantics

Reads bit zero at PON SerDes PLL offset `0x10`, logs the normalized value, and
returns it.

## Caller Context

No direct code xrefs target this entry; it may be an external PLL status API.

## Evidence

- Complete ARM64 body at `0x1870` through `0x18a8`.
- Exact offset, bit mask, log string, preserved return, and no direct xrefs.
- IDA type at `0x1870` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
