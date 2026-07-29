# 0x01aa8 an1_pll_out_mode_get

## Status

- Status: complete
- Confidence: verified PLL offset/bit, both log branches, unsigned return, and
  absence of direct code xrefs.
- Size: `0x48` bytes, 16 ARM64 instructions.
- Recovered signature: `u32 an1_pll_out_mode_get(void)`.

## Semantics

Reads bit one at PLL offset `0x1c`. A set bit logs 156.25 MHz; a clear bit logs
155.52 MHz. Returns the normalized bit.

## Caller Context

No direct code xrefs target this entry; it may be an external PLL status API.

## Evidence

- Complete ARM64 body at `0x1aa8` through `0x1aec`.
- Exact offset, bit extraction, strings, branches, and preserved return.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1aa8` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
