# 0x00188 pon_set_pll_pon_en

## Status

- Status: complete
- Confidence: verified low-bit input, CRM bit-28 RMW, zero return, and absence
  of direct code xrefs.
- Size: `0x24` bytes, 9 ARM64 instructions.
- Recovered signature: `int pon_set_pll_pon_en(u8 enable)`.

## Semantics

Replaces bit 28 at `top_crm_base + 0xc4` with input bit zero, then returns zero.

## Caller Context

No direct code xrefs target this entry. It may be an exported or indirect PON
PLL control API.

## Evidence

- Complete ARM64 body at `0x188` through `0x1a8`.
- Exact `UBFIZ #28,#1`, CRM offset `0xc4`, and mask `0xefffffff`.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x188` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
