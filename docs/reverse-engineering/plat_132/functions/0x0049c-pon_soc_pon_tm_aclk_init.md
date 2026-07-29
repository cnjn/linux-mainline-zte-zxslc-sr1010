# 0x0049c pon_soc_pon_tm_aclk_init

## Status

- Status: complete
- Confidence: verified CPU predicate, both CRM RMW values, zero return, and sole
  direct caller.
- Size: `0x38` bytes, 14 ARM64 instructions.
- Recovered signature: `int pon_soc_pon_tm_aclk_init(void)`.

## Semantics

At CRM offset `0xc`, CPU 129 sets bits zero and one; every other CPU type sets
bits zero through two. The function returns zero.

## Caller Context

Its sole direct caller is `zx_pon_probe @ 0x580`.

## Evidence

- Complete ARM64 body at `0x49c` through `0x4d0`.
- Exact CPU-129 predicate, `ORR #3`/`ORR #7`, CRM offset, and caller xref.
- IDA type at `0x49c` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
