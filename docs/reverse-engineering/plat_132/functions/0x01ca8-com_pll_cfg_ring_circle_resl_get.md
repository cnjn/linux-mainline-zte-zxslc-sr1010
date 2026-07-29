# 0x01ca8 com_pll_cfg_ring_circle_resl_get

## Status

- Status: complete
- Confidence: verified SerDes offset/field, original log string, unsigned return,
  and absence of direct code xrefs.
- Size: `0x3c` bytes, 14 ARM64 instructions.
- Recovered signature: `u32 com_pll_cfg_ring_circle_resl_get(void)`.

## Semantics

Extracts bits 23-26 at common SerDes offset `0x4`, logs the value using the
binary's `an1_`-prefixed string, and returns the four-bit value.

## Caller Context

No direct code xrefs target this entry; it may be an external PLL tuning API.

## Evidence

- Complete ARM64 body at `0x1ca8` through `0x1ce0`.
- Exact offset, extraction, original string, and preserved return.
- IDA type at `0x1ca8` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
