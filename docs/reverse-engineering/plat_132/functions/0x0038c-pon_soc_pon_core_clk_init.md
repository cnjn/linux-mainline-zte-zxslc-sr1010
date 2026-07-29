# 0x0038c pon_soc_pon_core_clk_init

## Status

- Status: complete
- Confidence: verified CPU gates, revision-specific CRM RMW, zero return, and
  sole direct caller.
- Size: `0x54` bytes, 21 ARM64 instructions.
- Recovered signature: `int pon_soc_pon_core_clk_init(void)`.

## Semantics

The function does nothing outside CPU types 133 and 129. For CPU 133 it sets
CRM offset `0xc` bits 4-6. For CPU 129 it replaces bits 4-5 with binary two.
It returns zero in every path.

## Caller Context

Its sole direct caller is `zx_pon_probe @ 0x580`.

## Evidence

- Complete ARM64 body at `0x38c` through `0x3dc`.
- Exact CPU predicate gate, CRM offset `0xc`, masks, constants, and caller xref.
- IDA type at `0x38c` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
