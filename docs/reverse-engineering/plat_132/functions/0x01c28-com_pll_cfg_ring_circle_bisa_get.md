# 0x01c28 com_pll_cfg_ring_circle_bisa_get

## Status

- Status: complete
- Confidence: verified SerDes offset/field, both logs, unsigned return, and
  absence of direct code xrefs.
- Size: `0x48` bytes, 16 ARM64 instructions.
- Recovered signature: `u32 com_pll_cfg_ring_circle_bisa_get(void)`.

## Semantics

Extracts bits 16-19 at common SerDes offset `0x4`, logs the value using the
binary's `an1_`-prefixed string, prints the complete 0-375u mapping, and returns
the four-bit value.

## Caller Context

No direct code xrefs target this entry; it may be an external PLL tuning API.

## Evidence

- Complete ARM64 body at `0x1c28` through `0x1c6c`.
- Exact offset, extraction, both strings, and preserved return.
- IDA type at `0x1c28` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
