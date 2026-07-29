# 0x000dc pon_set_1pps_tod_out_en

## Status

- Status: complete
- Confidence: verified byte input, both ordered child calls, ignored child
  returns, zero return, and no direct code xrefs.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature: `int pon_set_1pps_tod_out_en(u8 enable)`.

## Semantics

Passes the same byte-truncated enable value first to `pon_set_1pps_out_en` and
then to `pon_set_uart1_txd_en`. It ignores both results and returns zero.

## Caller Context

No direct code xrefs target this entry. It may be an exported or indirect PON
ToD output API.

## Evidence

- Complete ARM64 body at `0xdc` through `0x100`.
- Preserved byte argument across both ordered calls and explicit zero return.
- Exhaustive direct xref query reported no code references.
- IDA type at `0xdc` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
