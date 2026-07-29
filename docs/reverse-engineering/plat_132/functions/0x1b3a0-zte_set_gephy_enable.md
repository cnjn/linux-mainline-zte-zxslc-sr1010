# 0x1b3a0 zte_set_gephy_enable

## Status

- Status: complete
- Confidence: verified byte ABI, exact equality-one predicate, register-zero
  bit-11 RMW, zero return, and SMAC init table reference.
- Size: `0x54` bytes, 21 ARM64 instructions.
- Recovered signature: `int zte_set_gephy_enable(u8 phy, u8 enable)`.

## Semantics

The helper reads MDIO register zero. Only `enable == 1` clears bit 11; every
other byte value sets bit 11. It writes the result to register zero and returns
zero.

## Caller Context

One data reference from `nppt_smac_init @ 0x129c8` populates this function in a
GEPHY callback table. No direct `BL` caller exists.

## Concurrency and Ownership

No local lock, allocation, cleanup, or MDIO error handling exists. The register
RMW is not internally serialized.

## Evidence

- Complete ARM64 body at `0x1b3a0` through `0x1b3f0`.
- Byte input conversions, exact comparison with one, masks `0xf7ff`/`0x0800`,
  and register-zero read/write.
- One data xref from the SMAC initialization callback setup.
- IDA type at `0x1b3a0` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Whether register-zero bit 11 is active-low PHY power or enable control.
