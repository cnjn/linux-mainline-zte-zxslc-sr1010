# 0x1aca4 zte_gephy_get_eee_en_status

## Status

- Status: complete
- Confidence: verified null handling, byte output, MDIO selection/read, bit
  extraction, returns, and no direct xrefs.
- Size: `0x60` bytes, 23 ARM64 instructions.
- Recovered signature: `int zte_gephy_get_eee_en_status(u8 phy, u8 *status)`.

## Semantics

The function rejects a null status pointer, logs `"eee_en_status is null\n"`,
and returns `-1`. With a valid pointer it writes `0xffff8001` to MDIO register
16, extracts bits one and two from register 17 into the output byte, and returns
zero.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY control path; direct xrefs cannot establish that.

## Concurrency and Ownership

The caller owns the output storage. No local lock or MDIO error handling exists.

## Evidence

- Complete ARM64 body at `0x1aca4` through `0x1ad00`.
- Direct null branch and exact log string.
- MDIO register-16 write, register-17 `UBFX #1,#2`, byte output store, and
  zero/-1 return paths.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1aca4` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware semantics of the selected register-17 field.
