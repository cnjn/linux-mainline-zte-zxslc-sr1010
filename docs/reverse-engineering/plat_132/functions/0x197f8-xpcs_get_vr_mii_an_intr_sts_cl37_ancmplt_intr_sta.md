# 0x197f8 xpcs_get_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta

## Status

- Status: complete
- Confidence: verified selector/address branches, normalized bit-zero read,
  output store, return value, and absence of direct code xrefs.
- Size: `0x48` bytes, 17 ARM64 instructions.
- Recovered signature:
  `u32 xpcs_get_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta(u8 xmac, u32 *status)`.

## Semantics

The function selects PCS offset `0x7e0008`, reads it, masks bit zero, stores the
normalized result through `status`, and returns that same result. It does not
validate the output pointer.

Selectors two and three use the raw `0x7e0008 + (xmac << 23)` window. Other
selectors use `xmac0_pcs_base + 0x7e0008 + sign_extend32(xmac << 24)`.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect caller; that cannot be established from direct xrefs.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The MMIO read and
caller-owned output write are unsynchronized by this helper.

## Evidence

- Complete ARM64 body at `0x197f8` through `0x1983c`.
- Exact `LDR`, `AND #1`, output `STR`, and return-register behavior.
- Both raw and `xmac0_pcs_base`-relative selector paths.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x197f8` updated to the recovered `(u8, u32 *) -> u32` signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Whether this helper is part of an exported or indirect PCS callback surface.
