# 0x0014c pon_set_pll_pon_cfg_with_ref_clk_25M

## Status

- Status: complete
- Confidence: verified each fixed CRM write, final bit-28 RMW, zero return, and
  sole direct caller.
- Size: `0x3c` bytes, 14 ARM64 instructions.
- Recovered signature: `int pon_set_pll_pon_cfg_with_ref_clk_25M(void)`.

## Semantics

Writes the fixed PLL profile at CRM offsets `0xc0` through `0xcc`: `0x102371`,
`0x0a000000`, `0x20`, and zero. It then sets bit 28 at `0xc4` and returns zero.

## Caller Context

Its sole direct caller is `pon_use_pll_pon_ref_from_ex_pll @ 0x1ac`.

## Evidence

- Complete ARM64 body at `0x14c` through `0x184`.
- Exact offsets, values, final bit-28 RMW, and caller xref.
- IDA type at `0x14c` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
