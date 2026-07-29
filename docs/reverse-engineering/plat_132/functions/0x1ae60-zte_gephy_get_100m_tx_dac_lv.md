# 0x1ae60 zte_gephy_get_100m_tx_dac_lv

## Status

- Status: complete
- Confidence: verified null handling, 16-bit output, MDIO selection/read, mask,
  returns, and no direct xrefs.
- Size: `0x60` bytes, 23 ARM64 instructions.
- Recovered signature: `int zte_gephy_get_100m_tx_dac_lv(u8 phy, u16 *level)`.

## Semantics

The function rejects a null output pointer, logs `"tx_dac_lv is null\n"`, and
returns `-1`. Otherwise it writes `0xffffb406` to MDIO register 16, reads
register 17, stores the low six bits through the 16-bit output pointer, and
returns zero.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY control path; direct xrefs cannot establish that.

## Concurrency and Ownership

The caller owns output storage. No local lock or MDIO error handling exists.

## Evidence

- Complete ARM64 body at `0x1ae60` through `0x1aebc`.
- Null branch and exact log string.
- Register-16 selection value `0xffffb406`, register-17 `0x3f` mask, halfword
  output store, and zero/-1 returns.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1ae60` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware semantics and valid range of the selected six-bit field.
