# 0x00480 pon_soc_pon_cci_aclk_init

## Status

- Status: complete
- Confidence: verified CRM RMW, zero return, and sole direct caller.
- Size: `0x1c` bytes, 7 ARM64 instructions.
- Recovered signature: `int pon_soc_pon_cci_aclk_init(void)`.

## Semantics

Sets bits 4-6 at `top_crm_base + 0x4`, then returns zero.

## Caller Context

Its sole direct caller is `zx_pon_probe @ 0x580`.

## Evidence

- Complete ARM64 body at `0x480` through `0x498`.
- Exact CRM offset `0x4`, `ORR #0x70`, store, return, and caller xref.
- IDA type at `0x480` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
