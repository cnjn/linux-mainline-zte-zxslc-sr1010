# 0x188f0 xpcs_set_sr_xs_pcs_ctrl2_pcs_type

## Status

- Status: complete
- Confidence: verified selector/address branches, full-word direct store, void
  return, and all direct callers.
- Size: `0x40` bytes, 15 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_sr_xs_pcs_ctrl2_pcs_type(u8 xmac, u32 pcs_type)`.

## Semantics

The function truncates `xmac` to a byte, selects the PCS word at offset
`0x0c001c`, and directly writes the complete raw `pcs_type` word. It does not
read the prior register value or mask the input.

Selectors two and three use raw address `0x0c001c + (xmac << 23)`. Other
selectors use `xmac0_pcs_base + 0x0c001c + sign_extend32(xmac << 24)`.

## Caller Context

Six configuration paths call the helper: 1G, 10GBASE-R, 5GBASE-R, 2.5GBASE-X,
HSGMII, and USXGMII. Their values are mode-specific literals or configuration
arguments; no caller consumes its incidental pointer return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The direct volatile
store is not internally serialized.

## Evidence

- Complete 15-instruction ARM64 body at `0x188f0` through `0x1892c`.
- Both direct and `xmac0_pcs_base`-relative pointer paths.
- Final `STR W1,[X0]` without a preceding register read or mask.
- Exhaustive direct xref query found six caller sites.
- IDA type at `0x188f0` updated to the recovered void `(u8, u32)` signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact hardware decoding of the raw PCS type word.
