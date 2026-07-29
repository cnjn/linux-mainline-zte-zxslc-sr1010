# 0x01c70 com_pll_cfg_ring_circle_resl_set

## Status

- Status: complete
- Confidence: verified SerDes offset/field, unmasked input shift, RMW, log,
  return, and absence of direct code xrefs.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int com_pll_cfg_ring_circle_resl_set(u32 value)`.

## Semantics

Clears bits 23-26 at common SerDes offset `0x4`, ORs the complete input shifted
left by 23, then returns the logging result. Inputs above four bits can alter
higher register bits.

## Caller Context

No direct code xrefs target this entry; it may be an external PLL tuning API.

## Evidence

- Complete ARM64 body at `0x1c70` through `0x1ca4`.
- Exact offset, mask, unmasked shift, log string, and tail return.
- IDA type at `0x1c70` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
