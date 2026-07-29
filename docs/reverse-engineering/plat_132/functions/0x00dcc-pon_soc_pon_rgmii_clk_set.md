# 0x00dcc pon_soc_pon_rgmii_clk_set

## Status

- Status: complete
- Confidence: verified integer predicate, active-low CRM bit replacement, zero
  return, and sole direct caller.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature: `int pon_soc_pon_rgmii_clk_set(int enable)`.

## Semantics

Replaces CRM offset `0xc` bit 16 with `enable == 0`: zero input sets the bit,
while every nonzero input clears it. The function returns zero.

## Caller Context

Its sole direct caller is `nppt_smac_set_rgmii_mode @ 0x12980`.

## Evidence

- Complete ARM64 body at `0xdcc` through `0xdf0`.
- Exact integer-zero predicate, clear mask `0xfffeffff`, set bit `0x10000`, and
  caller xref.
- IDA type at `0xdcc` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
