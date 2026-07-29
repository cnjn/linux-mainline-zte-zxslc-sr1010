# 0x011fc epon_set_dg_cnt

## Status

- Status: complete
- Confidence: verified void setter ABI, both independent work-mode branches,
  offsets, nibble transformation, and sole direct caller.
- Size: `0x5c` bytes, 23 ARM64 instructions.
- Recovered signature: `void epon_set_dg_cnt(void)`.

## Semantics

For work-mode mask `0xa0`, transforms the word at PON offset `0x1800f0`; for
bit `0x100`, independently transforms offset `0x1c0110`. Each transformation
clears the low nibble and ORs the original low nibble shifted left by one.

## Caller Context

Its sole direct caller is `zx_pon_int @ 0x12bc`, which ignores machine-register
residue after the setter call.

## Evidence

- Complete ARM64 body at `0x11fc` through `0x1254`.
- Exact mode masks, offsets, `UBFIZ #1,#4`, high-bit preserve mask, and caller
  xref.
- IDA type at `0x11fc` updated to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
