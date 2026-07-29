# 0x0035c pon_int_enable

## Status

- Status: complete
- Confidence: verified mask input, PON offset, clear-mask RMW, return value,
  and all direct callers.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `u32 pon_int_enable(u32 mask)`.

## Semantics

Reads PON offset `0x44`, clears every bit set in `mask`, writes the result to
the same register, and returns the updated word.

## Caller Context

Eight direct callers are interrupt registration helpers for GMAC, XGMAC, EMAC,
XEUMAC, XEDMAC, diagnostic, LP, and low-power interrupt paths.

## Evidence

- Complete ARM64 body at `0x35c` through `0x370`.
- Exact `BIC` mask operation, PON offset `0x44`, store, and return-register
  flow.
- Exhaustive direct xref query found eight registration callers.
- IDA type at `0x35c` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
