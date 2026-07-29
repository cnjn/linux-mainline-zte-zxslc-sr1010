# 0x1af6c zte_gephy_get_link_status_change_event

## Status

- Status: complete
- Confidence: verified null handling, 16-bit output, MDIO read, bit extraction,
  returns, and no direct xrefs.
- Size: `0x4c` bytes, 18 ARM64 instructions.
- Recovered signature:
  `int zte_gephy_get_link_status_change_event(u8 phy, u16 *occurred)`.

## Semantics

The function rejects a null output pointer, logs `"is_occurred is null\n"`, and
returns `-1`. Otherwise it reads MDIO register 25, stores normalized bit two
through a 16-bit output pointer, and returns zero.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY control path; direct xrefs cannot establish that.

## Concurrency and Ownership

The caller owns output storage. No local lock or MDIO error handling exists.

## Evidence

- Complete ARM64 body at `0x1af6c` through `0x1afb4`.
- Null branch and exact log string.
- Register-25 `UBFX #2,#1`, halfword output store, and zero/-1 returns.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1af6c` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Whether reading MDIO register 25 has acknowledge or clear-on-read semantics.
