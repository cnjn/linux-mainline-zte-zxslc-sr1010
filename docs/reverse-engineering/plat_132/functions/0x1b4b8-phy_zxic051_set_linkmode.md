# 0x1b4b8 phy_zxic051_set_linkmode

## Status

- Status: complete
- Confidence: verified byte ABI, wrapped/signed MDIO slot, PHY-ID failure path,
  all mode branches, indirect writes, return values, and callback-table reference.
- Size: `0x2dc` bytes, 175 ARM64 instructions.
- Recovered signature:
  `int phy_zxic051_set_linkmode(u8 phy, u8 force_mode, u8 duplex, u8 speed)`.

## Semantics

The helper forms `phy_with_offset = (u8)(phy + 4)`, obtains a PHY ID, and uses
the signed value `phy_with_offset - 4` to index two MDIO callback tables. PHY ID
`0xff` returns `-1`; a rate-limit gate controls logging of the error.

With `force_mode == 1`, it always runs the four-write script, selecting initial
value `0x2001` only for speed three and `0x2081` otherwise. Without force mode,
speed four selects `0x2081`; speed three selects `0x2001`; speed two and one
require duplex zero or one and choose mode values `0x1de1`/`0x1ce1` and
`0x1c61`/`0x1c21` respectively. Invalid non-forced speed or duplex values return
zero without writes. Every active script writes selector 7/value `0x3200` last.

## Caller Context

One data reference from `xmac_zxic_phy_init @ 0x18348` populates this helper in
a PHY callback table. No direct `BL` caller exists.

## Concurrency and Ownership

The callback tables and port/PHY-ID mapping are assumed initialized. No range
check protects the signed table slot and no local serialization exists.

## Evidence

- Complete ARM64 body at `0x1b4b8` through `0x1b790`.
- Byte wrapping of `phy + 4`, `0xff` failure sentinel, and ratelimited log.
- Full instruction-level branch reconstruction for force mode, speeds 1-4,
  duplex values zero/one, all constants, and all indirect table calls.
- One data xref from the XMAC ZXIC PHY callback setup.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact hardware meaning of selectors 7/9 and the programmed constants.
