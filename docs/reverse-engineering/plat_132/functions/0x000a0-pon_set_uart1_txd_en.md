# 0x000a0 pon_set_uart1_txd_en

## Status

- Status: complete
- Confidence: verified byte input, pin-mux RMW branches, zero return, and sole
  direct caller.
- Size: `0x3c` bytes, 15 ARM64 instructions.
- Recovered signature: `int pon_set_uart1_txd_en(u8 enable)`.

## Semantics

Reads the base pin-mux word. Only `enable == 1` clears bits 27-28. Other values
only write when those bits are zero, setting bit 28. Every path returns zero.

## Caller Context

Its sole direct caller is `pon_set_1pps_tod_out_en @ 0xdc`.

## Evidence

- Complete ARM64 body at `0xa0` through `0xd8`.
- Exact byte conversion, equality-one branch, field test, masks, and caller
  xref.
- IDA type at `0xa0` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
