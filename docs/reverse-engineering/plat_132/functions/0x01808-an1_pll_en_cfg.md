# 0x01808 an1_pll_en_cfg

## Status

- Status: complete
- Confidence: verified PLL offset, raw 32-bit input behavior, RMW, return word,
  and absence of direct code xrefs.
- Size: `0x1c` bytes, 7 ARM64 instructions.
- Recovered signature: `u32 an1_pll_en_cfg(u32 value)`.

## Semantics

Clears bit zero at PON SerDes PLL offset `0x10`, ORs the complete unmasked
`value`, writes and returns the updated word.

## Caller Context

No direct code xrefs target this entry; it may be an external PLL control API.

## Evidence

- Complete ARM64 body at `0x1808` through `0x1820`.
- Exact offset, clear mask, unmasked W0 OR, store, and return flow.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1808` updated to the recovered unsigned signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
