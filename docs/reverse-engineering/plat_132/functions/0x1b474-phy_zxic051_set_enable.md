# 0x1b474 phy_zxic051_set_enable

## Status

- Status: complete
- Confidence: verified byte inputs, u32 state-array write, no-argument notifier
  call, zero return, and PHY callback-table reference.
- Size: `0x30` bytes, 12 ARM64 instructions.
- Recovered signature: `int phy_zxic051_set_enable(u8 phy, u8 enable)`.

## Semantics

The function truncates both inputs to bytes, stores the enable byte zero-extended
as a full word at `outerphy_link[phy + 12]`, calls the no-argument external
`phy_zxic_051_set_enable`, and returns zero.

## Caller Context

One data reference from `xmac_zxic_phy_init @ 0x18348` populates this helper in
a PHY callback table. No direct `BL` caller exists.

## Concurrency and Ownership

The state-array update and notifier call are unsynchronized. No index bounds
check or error handling exists.

## Evidence

- Complete ARM64 body at `0x1b474` through `0x1b4a0`.
- Byte conversions, index scaling, word offset `0x30`, full word store, and
  no-argument notifier call.
- One data xref from the XMAC ZXIC PHY callback setup.
- IDA type at `0x1b474` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Effects and synchronization contract of the external notifier.
