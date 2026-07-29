# 0x1af20 zte_gephy_get_link_status_change_en

## Status

- Status: complete
- Confidence: verified null handling, 16-bit output, MDIO read, bit extraction,
  returns, and no direct xrefs.
- Size: `0x4c` bytes, 18 ARM64 instructions.
- Recovered signature:
  `int zte_gephy_get_link_status_change_en(u8 phy, u16 *enable)`.

## Semantics

The function rejects a null output pointer, logs `"en is null\n"`, and returns
`-1`. Otherwise it reads MDIO register 24, stores normalized bit two through a
16-bit output pointer, and returns zero.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY control path; direct xrefs cannot establish that.

## Concurrency and Ownership

The caller owns output storage. No local lock or MDIO error handling exists.

## Evidence

- Complete ARM64 body at `0x1af20` through `0x1af68`.
- Null branch and exact log string.
- Register-24 `UBFX #2,#1`, halfword output store, and zero/-1 returns.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1af20` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware semantics of MDIO register-24 bit two.
