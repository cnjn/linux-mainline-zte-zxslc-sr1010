# 0x1b28c check_phy_gephy

## Status

- Status: complete
- Confidence: verified MDIO page save/restore, discarded read, delay, status
  mapping, return values, and SMAC init table references.
- Size: `0xb4` bytes, 44 ARM64 instructions.
- Recovered signature: `int check_phy_gephy(u8 phy)`.

## Semantics

The helper saves MDIO register 30, writes zero to it, reads register 26 once
and discards the result, delays `429500`, then performs a second 16-bit register
26 read and restores register 30. A clear bit six returns `-1`. Otherwise it
maps bits 8-9 to low return bits as zero to one, one to two, two to three, and
three to seven; it ORs bit seven into return bit 10.

## Caller Context

Two data references from `nppt_smac_init @ 0x129c8` populate this function in a
GEPHY callback table. No direct `BL` caller exists.

## Concurrency and Ownership

No local lock, allocation, cleanup, or MDIO error handling exists. Register 30
is restored before the status result is examined.

## Evidence

- Complete ARM64 body at `0x1b28c` through `0x1b33c`.
- Exact register-30 save/zero/restore, discarded initial register-26 read,
  delay constant, and final `UXTH` conversion.
- Bit-six gate and exact status-field mapping.
- Two data xrefs from the SMAC initialization function-pointer setup.
- IDA type at `0x1b28c` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- MDIO register-30 page semantics and the reason for the discarded read.
