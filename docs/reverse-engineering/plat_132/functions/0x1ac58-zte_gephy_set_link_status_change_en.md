# 0x1ac58 zte_gephy_set_link_status_change_en

## Status

- Status: complete
- Confidence: verified byte/halfword ABI, register selection, low-bit mapping,
  bit-two RMW, zero return, and no direct xrefs.
- Size: `0x4c` bytes, 19 ARM64 instructions.
- Recovered signature:
  `int zte_gephy_set_link_status_change_en(u8 phy, u16 enable)`.

## Semantics

The function truncates `phy` to a byte and `enable` to a halfword. It reads MDIO
register 24, replaces bit two with bit zero of `enable`, writes the result to
the same register, and returns zero.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY control path; direct xrefs cannot establish that.

## Concurrency and Ownership

No local lock, allocation, cleanup, or error checking. The MDIO RMW is not
internally serialized.

## Evidence

- Complete ARM64 body at `0x1ac58` through `0x1aca0`.
- `UXTH` parameter conversion, `UBFIZ #2,#1`, `0xfffffffb` mask, and MDIO
  register 24 read/write.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1ac58` updated to the recovered integer-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware semantics of MDIO register-24 bit two.
