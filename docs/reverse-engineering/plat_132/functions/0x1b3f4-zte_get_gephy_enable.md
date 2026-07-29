# 0x1b3f4 zte_get_gephy_enable

## Status

- Status: complete
- Confidence: verified wide first argument, unchecked byte output, active-low
  bit test, zero return, and SMAC init table reference.
- Size: `0x3c` bytes, 15 ARM64 instructions.
- Recovered signature: `int zte_get_gephy_enable(uintptr_t phy, u8 *enable)`.

## Semantics

The helper reads MDIO register zero and stores one when bit 11 is clear or zero
when it is set. It unconditionally dereferences the output pointer and returns
zero. The 64-bit first argument is not truncated before the MDIO call in the
binary; the MDIO call ABI consumes its low register portion.

## Caller Context

One data reference from `nppt_smac_init @ 0x129c8` populates this function in a
GEPHY callback table. No direct `BL` caller exists.

## Concurrency and Ownership

The caller owns the non-null output storage. No local lock or MDIO error
handling exists.

## Evidence

- Complete ARM64 body at `0x1b3f4` through `0x1b42c`.
- No `UXTB` before the register-zero MDIO read, bit-11 test, and byte stores.
- One data xref from the SMAC initialization callback setup.
- IDA type at `0x1b3f4` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Whether the wide first argument represents a legacy callback ABI.
