# 0x00104 pon_set_pin_mux_13

## Status

- Status: complete
- Confidence: verified low-two-bit input, pin-mux field RMW, zero return, and
  sole direct caller.
- Size: `0x24` bytes, 9 ARM64 instructions.
- Recovered signature: `int pon_set_pin_mux_13(u8 value)`.

## Semantics

Replaces base pin-mux bits 25-26 with the low two bits of `value`, then returns
zero.

## Caller Context

Its sole direct caller is `pon_use_pll_pon_ref_from_ex_pll @ 0x1ac`.

## Evidence

- Complete ARM64 body at `0x104` through `0x124`.
- Exact `UBFIZ #25,#2`, mask `0xf9ffffff`, store, and caller xref.
- IDA type at `0x104` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
