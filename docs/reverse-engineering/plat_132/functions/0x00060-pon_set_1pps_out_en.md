# 0x00060 pon_set_1pps_out_en

## Status

- Status: complete
- Confidence: verified byte input, pin-mux RMW branches, zero return, and sole
  direct caller.
- Size: `0x40` bytes, 16 ARM64 instructions.
- Recovered signature: `int pon_set_1pps_out_en(u8 enable)`.

## Semantics

Reads `pin_mux_base + 8`. Only `enable == 1` replaces bits 21-23 with binary
two. Other inputs only write when the existing field equals two, setting all
three field bits. Every path returns zero.

## Caller Context

Its sole direct caller is `pon_set_1pps_tod_out_en @ 0xdc`.

## Evidence

- Complete ARM64 body at `0x60` through `0x9c`.
- Exact byte conversion, equality-one branch, bits 21-23 extraction, masks,
  and caller xref.
- IDA type at `0x60` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
