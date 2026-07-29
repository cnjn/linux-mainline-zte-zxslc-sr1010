# 0x1b1a4 zte_gephy_set_100m_tx_dac_slew

## Status

- Status: complete
- Confidence: verified byte/halfword ABI, range gate, MDIO selection/RMW,
  return paths, and no direct xrefs.
- Size: `0x74` bytes, 29 ARM64 instructions.
- Recovered signature:
  `int zte_gephy_set_100m_tx_dac_slew(u8 phy, u16 slew)`.

## Semantics

The function truncates `phy` to a byte and `slew` to a halfword. A slew above
seven logs `"tx_dac_slew is error. max is %u\n"` with argument seven and returns
`-1`. Otherwise it selects MDIO register 17 by writing `0xffffb408` to register
16, replaces register-17 low three bits with `slew`, and returns zero.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY control path; direct xrefs cannot establish that.

## Concurrency and Ownership

No local lock, allocation, cleanup, or MDIO error handling exists. The MDIO RMW
is not internally serialized.

## Evidence

- Complete ARM64 body at `0x1b1a4` through `0x1b214`.
- `UXTH` slew conversion, unsigned comparison to seven, exact error string,
  selector value `0xffffb408`, and `0xfff8` RMW mask.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1b1a4` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware calibration semantics of the three-bit DAC slew field.
