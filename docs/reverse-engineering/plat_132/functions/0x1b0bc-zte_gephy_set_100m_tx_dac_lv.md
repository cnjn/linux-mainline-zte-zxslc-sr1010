# 0x1b0bc zte_gephy_set_100m_tx_dac_lv

## Status

- Status: complete
- Confidence: verified byte/halfword ABI, range gate, MDIO selection/RMW,
  return paths, and no direct xrefs.
- Size: `0x74` bytes, 29 ARM64 instructions.
- Recovered signature: `int zte_gephy_set_100m_tx_dac_lv(u8 phy, u16 level)`.

## Semantics

The function truncates `phy` to a byte and `level` to a halfword. A level above
63 logs `"tx_dac_lv is error. max is %u\n"` with argument 63 and returns `-1`.
Otherwise it selects MDIO register 17 by writing `0xffffb406` to register 16,
replaces register-17 low six bits with `level`, and returns zero.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY control path; direct xrefs cannot establish that.

## Concurrency and Ownership

No local lock, allocation, cleanup, or MDIO error handling exists. The MDIO RMW
is not internally serialized.

## Evidence

- Complete ARM64 body at `0x1b0bc` through `0x1b12c`.
- `UXTH` level conversion, unsigned comparison to `0x3f`, exact error string,
  selector value `0xffffb406`, and `0xffc0` RMW mask.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1b0bc` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware calibration semantics of the six-bit DAC level.
