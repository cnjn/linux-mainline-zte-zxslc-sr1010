# 0x00548 pon_soc_pon_woe_clk_init

## Status

- Status: complete
- Confidence: verified CPU predicate, both CRM RMW values, zero return, and sole
  direct caller.
- Size: `0x38` bytes, 14 ARM64 instructions.
- Recovered signature: `int pon_soc_pon_woe_clk_init(void)`.

## Semantics

At CRM offset `0xc`, CPU 129 sets bits covered by `0x000c0000`; every other CPU
type sets bits covered by `0x00700000`. The function returns zero.

## Caller Context

Its sole direct caller is `zx_pon_probe @ 0x580`.

## Evidence

- Complete ARM64 body at `0x548` through `0x57c`.
- Exact CPU-129 predicate, OR constants, CRM offset, and caller xref.
- IDA type at `0x548` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
