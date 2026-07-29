# 0x01a38 an1_pll_bypass_get

## Status

- Status: complete
- Confidence: verified PLL offset/bit, log, unsigned return, and absence of
  direct code xrefs.
- Size: `0x3c` bytes, 14 ARM64 instructions.
- Recovered signature: `u32 an1_pll_bypass_get(void)`.

## Semantics

Reads bit seven at PON SerDes PLL offset `0xc`, logs the normalized value, and
returns it. It reports bypass enabled/disabled, not bypass mode one versus two.

## Caller Context

No direct code xrefs target this entry; it may be an external PLL status API.

## Evidence

- Complete ARM64 body at `0x1a38` through `0x1a70`.
- Exact offset, `UBFX #7,#1`, log string, and preserved return.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1a38` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
