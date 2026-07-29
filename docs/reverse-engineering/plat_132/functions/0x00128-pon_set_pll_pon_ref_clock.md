# 0x00128 pon_set_pll_pon_ref_clock

## Status

- Status: complete
- Confidence: verified low-two-bit input, CRM RMW field, zero return, and sole
  direct caller.
- Size: `0x24` bytes, 9 ARM64 instructions.
- Recovered signature: `int pon_set_pll_pon_ref_clock(u8 clock)`.

## Semantics

Replaces `top_crm_base + 0x10` bits 4-5 with the low two bits of `clock`, then
returns zero.

## Caller Context

Its sole direct caller is `pon_use_pll_pon_ref_from_ex_pll @ 0x1ac`.

## Evidence

- Complete ARM64 body at `0x128` through `0x148`.
- Exact `UBFIZ #4,#2`, CRM offset `0x10`, mask `0xffffffcf`, and caller xref.
- IDA type at `0x128` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
