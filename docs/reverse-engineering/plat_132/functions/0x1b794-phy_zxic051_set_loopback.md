# 0x1b794 phy_zxic051_set_loopback

## Status

- Status: complete
- Confidence: verified byte ABI, wrapped/signed MDIO slot, PHY-ID error path,
  both indirect RMW scripts, delay positions, and callback-table reference.
- Size: `0x24c` bytes, 142 ARM64 instructions.
- Recovered signature: `int phy_zxic051_set_loopback(u8 phy, u8 enable)`.

## Semantics

The helper computes `(u8)(phy + 4)`, resolves a PHY ID, and indexes indirect
MDIO callback tables with the signed value `phy_with_offset - 4`. ID `0xff`
returns `-1` after a rate-limited optional log.

When `enable == 1`, the script applies register-0 mask/set `| 0x4900`, clears
extended register 7 bit 12, clears extended register 1 bits from `0x2040` while
setting bit 6, clears register-0 bit 11, and delays `4295000`. Other enable
values apply the complementary register-0 `& 0xb6ff | 0x800` write, delay,
set extended register 7 bit 12 and extended register 1 bits `0x2040`, delay,
then clear register-0 bit 11. Successful scripts return zero.

## Caller Context

One data reference from `xmac_zxic_phy_init @ 0x18348` populates this helper in
a PHY callback table. No direct `BL` caller exists.

## Concurrency and Ownership

No local lock protects PHY-ID mapping, callback tables, or multi-write scripts.

## Evidence

- Complete ARM64 body at `0x1b794` through `0x1b9dc`.
- Exact byte wrapping, failure sentinel/log, indirect callback table calls,
  RMW masks, and both `4295000` delay positions.
- One data xref from the XMAC ZXIC PHY callback setup.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware role of the loopback register fields and raw enable convention.
