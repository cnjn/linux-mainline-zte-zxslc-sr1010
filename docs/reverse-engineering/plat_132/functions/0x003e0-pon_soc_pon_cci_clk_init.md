# 0x003e0 pon_soc_pon_cci_clk_init

## Status

- Status: complete
- Confidence: verified CRM RMW, zero return, and absence of direct code xrefs.
- Size: `0x1c` bytes, 7 ARM64 instructions.
- Recovered signature: `int pon_soc_pon_cci_clk_init(void)`.

## Semantics

Sets bits 4-5 at `top_crm_base + 0x4`, then returns zero.

## Evidence

- Complete ARM64 body at `0x3e0` through `0x3f8`.
- Exact CRM offset `0x4`, `ORR #0x30`, store, and zero return.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x3e0` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
