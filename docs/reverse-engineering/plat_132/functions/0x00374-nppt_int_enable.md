# 0x00374 nppt_int_enable

## Status

- Status: complete
- Confidence: verified mask input, NPPT offset, clear-mask RMW, return value,
  and all direct callers.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `u32 nppt_int_enable(u32 mask)`.

## Semantics

Reads NPPT offset `0x4`, clears every bit set in `mask`, writes the result to
the same register, and returns the updated word.

## Caller Context

Three direct callers are PTP, PTP timestamp, and OAM interrupt registration
helpers.

## Evidence

- Complete ARM64 body at `0x374` through `0x388`.
- Exact `BIC` mask operation, NPPT offset `0x4`, store, return flow, and caller
  xrefs.
- IDA type at `0x374` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
