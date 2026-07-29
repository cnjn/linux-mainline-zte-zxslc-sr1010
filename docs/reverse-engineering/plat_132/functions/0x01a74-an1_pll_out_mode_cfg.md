# 0x01a74 an1_pll_out_mode_cfg

## Status

- Status: complete
- Confidence: verified PLL offset, unmasked input shift, RMW, log, return, and
  absence of direct code xrefs.
- Size: `0x34` bytes, 12 ARM64 instructions.
- Recovered signature: `int an1_pll_out_mode_cfg(u32 enable)`.

## Semantics

Clears bit one at PLL offset `0x1c`, ORs the complete input shifted left by one,
then returns the fixed explanatory `printk` result. Inputs above one can alter
bits above bit one.

## Caller Context

No direct code xrefs target this entry; it may be an external PLL control API.

## Evidence

- Complete ARM64 body at `0x1a74` through `0x1aa4`.
- Exact offset, clear mask, unmasked shifted input, string, and tail return.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1a74` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
