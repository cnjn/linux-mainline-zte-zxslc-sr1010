# 0x00418 pon_soc_pon_woe1_clk_init

## Status

- Status: complete
- Confidence: verified CRM RMW, zero return, and absence of direct code xrefs.
- Size: `0x1c` bytes, 7 ARM64 instructions.
- Recovered signature: `int pon_soc_pon_woe1_clk_init(void)`.

## Semantics

Sets bits covered by `0x07000000` at `top_crm_base + 0xc`, then returns zero.

## Evidence

- Complete ARM64 body at `0x418` through `0x430`.
- Exact CRM offset `0xc`, OR value, store, and zero return.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x418` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
