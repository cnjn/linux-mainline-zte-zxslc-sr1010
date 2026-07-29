# 0x1b9e0 phy_zxic051_get_loopback

## Status

- Status: complete
- Confidence: verified byte ABI, wrapped/signed MDIO slot, PHY-ID error path,
  bit-14 extraction, output behavior, and callback-table reference.
- Size: `0x98` bytes, 37 ARM64 instructions.
- Recovered signature: `int phy_zxic051_get_loopback(u8 phy, u8 *enabled)`.

## Semantics

The helper computes `(u8)(phy + 4)`, resolves a PHY ID, and uses the signed
value `phy_with_offset - 4` to select a GE MDIO read callback. A PHY ID of
`0xff` optionally rate-limits an error log and returns `-1` without writing the
output. On success it stores normalized GE register-zero bit 14 to the
unchecked output byte and returns zero.

## Caller Context

One data reference from `xmac_zxic_phy_init @ 0x18348` populates this helper in
a PHY callback table. No direct `BL` caller exists.

## Concurrency and Ownership

The caller owns non-null output storage. No local lock protects PHY-ID mapping
or the callback table.

## Evidence

- Complete ARM64 body at `0x1b9e0` through `0x1ba74`.
- Exact byte wrapping, PHY-ID sentinel, rate-limited error path, signed table
  slot, register-zero read, and bit-14 byte output.
- One data xref from the XMAC ZXIC PHY callback setup.
- IDA type at `0x1b9e0` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Whether GE register-zero bit 14 is the complete loopback state.
