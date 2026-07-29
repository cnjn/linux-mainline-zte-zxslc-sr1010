# 0x00020 pon_set_8k_out_en

## Status

- Status: complete
- Confidence: verified byte input, pin-mux RMW branches, zero return, and sole
  direct caller.
- Size: `0x40` bytes, 16 ARM64 instructions.
- Recovered signature: `int pon_set_8k_out_en(u8 enable)`.

## Semantics

Reads the pin-mux word. Only `enable == 1` replaces bits 1-2 with binary one.
Other values only write when the existing bits 1-2 equal one, setting both bits.
Every path returns zero.

## Caller Context

Its sole direct caller is `pon_use_pll_pon_ref_from_ex_pll @ 0x1ac`.

## Evidence

- Complete ARM64 body at `0x20` through `0x5c`.
- Exact byte conversion, equality-one branch, field extraction, masks, and
  caller xref.
- IDA type at `0x20` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
