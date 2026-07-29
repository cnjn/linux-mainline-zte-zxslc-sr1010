# 0x01bd8 com_pll_cfg_ring_circle_bisa_set

## Status

- Status: complete
- Confidence: verified SerDes offset/field, unmasked input shift, both logs,
  tail return, and absence of direct code xrefs.
- Size: `0x50` bytes, 19 ARM64 instructions.
- Recovered signature: `int com_pll_cfg_ring_circle_bisa_set(u32 value)`.

## Semantics

Clears bits 16-19 at common SerDes offset `0x4`, ORs the complete input shifted
left by 16, prints the complete 0-375u field mapping, then returns the data-log
result. Inputs above four bits can alter higher register bits.

## Caller Context

No direct code xrefs target this entry; it may be an external PLL tuning API.

## Evidence

- Complete ARM64 body at `0x1bd8` through `0x1c24`.
- Exact offset, mask, unmasked shift, both strings, and final logging return.
- IDA type at `0x1bd8` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
