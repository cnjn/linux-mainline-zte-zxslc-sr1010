# 0x01af0 an1_pll_cfg_ring_circle_bisa_set

## Status

- Status: complete
- Confidence: verified PLL offset/field, unmasked input shift, RMW, log, return,
  and absence of direct code xrefs.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int an1_pll_cfg_ring_circle_bisa_set(u32 value)`.

## Semantics

Clears bits 16-19 at PLL offset `0x4`, ORs the complete input shifted left by
16, then returns the logging result. Inputs above four bits can alter higher
register bits.

## Caller Context

No direct code xrefs target this entry; it may be an external PLL tuning API.

## Evidence

- Complete ARM64 body at `0x1af0` through `0x1b24`.
- Exact offset, mask `0xfff0ffff`, unmasked shift, log string, and tail return.
- IDA type at `0x1af0` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
