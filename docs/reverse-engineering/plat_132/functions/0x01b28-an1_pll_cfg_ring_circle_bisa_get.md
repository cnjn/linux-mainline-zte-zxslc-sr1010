# 0x01b28 an1_pll_cfg_ring_circle_bisa_get

## Status

- Status: complete
- Confidence: verified PLL offset/field, log, unsigned return, and absence of
  direct code xrefs.
- Size: `0x3c` bytes, 14 ARM64 instructions.
- Recovered signature: `u32 an1_pll_cfg_ring_circle_bisa_get(void)`.

## Semantics

Extracts bits 16-19 at PLL offset `0x4`, logs the four-bit value, and returns it.

## Caller Context

No direct code xrefs target this entry; it may be an external PLL tuning API.

## Evidence

- Complete ARM64 body at `0x1b28` through `0x1b60`.
- Exact offset, `UBFX #16,#4`, log string, and preserved return.
- IDA type at `0x1b28` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
