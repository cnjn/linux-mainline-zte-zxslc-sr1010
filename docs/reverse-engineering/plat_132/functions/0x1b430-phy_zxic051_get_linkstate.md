# 0x1b430 phy_zxic051_get_linkstate

## Status

- Status: complete
- Confidence: verified output order, u32 array stride/index offsets, in-place
  speed conversion, zero return, and PHY callback-table reference.
- Size: `0x44` bytes, 17 ARM64 instructions.
- Recovered signature:
  `int phy_zxic051_get_linkstate(u8 phy, u8 *link, u8 *duplex, u8 *speed)`.

## Semantics

The helper truncates `phy` to a byte and reads low bytes from the contiguous
`outerphy_link` words at indices `phy`, `phy + 4`, and `phy + 8`. It stores them
to the unchecked link, speed, and duplex outputs respectively, then passes the
speed output pointer to `phy_zxic_speed_outer2uni` for in-place conversion. It
returns zero.

## Caller Context

One data reference from `xmac_zxic_phy_init @ 0x18348` populates this helper in
a PHY callback table. No direct `BL` caller exists.

## Concurrency and Ownership

The caller owns all non-null output pointers. Global PHY-state arrays are read
without synchronization.

## Evidence

- Complete ARM64 body at `0x1b430` through `0x1b470`.
- Byte PHY index scaled by four, word offsets `0`, `0x10`, and `0x20`.
- Output stores in link/speed/duplex order and direct in-place converter call.
- One data xref from the XMAC ZXIC PHY callback setup.
- IDA type at `0x1b430` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Whether the three contiguous array regions are independently updated.
