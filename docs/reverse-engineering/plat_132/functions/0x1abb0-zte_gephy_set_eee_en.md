# 0x1abb0 zte_gephy_set_eee_en

## Status

- Status: complete
- Confidence: verified byte arguments, MDIO register selection, exact
  equality-one predicate, bit-one/two RMW, zero return, and no direct xrefs.
- Size: `0x60` bytes, 24 ARM64 instructions.
- Recovered signature: `int zte_gephy_set_eee_en(u8 phy, u8 enable)`.

## Semantics

The function writes `0xffff8001` to MDIO register 16, reads register 17, clears
bits one and two with `0xfff9`, and sets both bits only when `enable == 1`.
It writes the result back to register 17 and returns zero. Values other than one
have the same clearing behavior as zero.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY control path; direct xrefs cannot establish that.

## Concurrency and Ownership

No local lock, allocation, cleanup, or error checking. The MDIO RMW is not
internally serialized.

## Evidence

- Complete ARM64 body at `0x1abb0` through `0x1ac0c`.
- Exact MDIO values and `CSEL` equality-one predicate.
- `0xfff9` clear mask and `0x0006` set bits.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1abb0` updated to the recovered integer-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware semantics of the register-16 page/select write and register-17 bits.
