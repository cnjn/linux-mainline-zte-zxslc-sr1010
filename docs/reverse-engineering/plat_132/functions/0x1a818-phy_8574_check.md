# 0x1a818 phy_8574_check

## Status

- Status: complete
- Confidence: verified basic-status gate, page save/restore, double read,
  status mapping, packed return, and absence of direct code xrefs.
- Size: `0xd8` bytes, 54 ARM64 instructions.
- Recovered signature: `int phy_8574_check(u8 phy)`.

## Semantics

The helper first reads extended MDIO register one and returns `-1` when bit two
is clear. Otherwise it saves register 31, writes zero to select a temporary
page, reads register 28 twice while discarding the first result, then restores
register 31 before interpreting the second 16-bit read.

It maps status bits 3-4 to low return bits: zero to one, one to two, two to
three, and three to seven. It ORs status bit five into return bit 10.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect PHY-dispatch path; direct xrefs cannot establish that.

## Concurrency and Ownership

No local lock, allocation, cleanup, or error checking. The function restores
the saved page before returning on the successful read path.

## Evidence

- Complete ARM64 body at `0x1a818` through `0x1a8ec`.
- Exact register-one bit-two gate, page register 31 save/write/restore, and
  duplicated register-28 reads.
- Exact bits-3/4 mapping and bit-five shift by ten.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1a818` updated to the recovered integer-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of page zero, status register 28, and the packed return.
- Reason the first temporary-page status read is discarded.
