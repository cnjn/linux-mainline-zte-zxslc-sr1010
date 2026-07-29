# 0x1b340 phy_zxicge_init

## Status

- Status: complete
- Confidence: verified unused byte callback ABI, guard behavior, contiguous global-slot
  layout, base offsets, return values, and SMAC init table reference.
- Size: `0x60` bytes, 24 ARM64 instructions.
- Recovered signature: `int phy_zxicge_init(u8 unused_phy)`.

## Semantics

The helper does not read its byte callback argument. It returns its unprotected
initialization byte unchanged when it is nonzero. On the first call it stores `gephy_apb_base` in contiguous APB-base
slot three, then stores `base + 0x400000`, `base + 0x300000`, and
`base + 0x100000` in slots zero, one, and two. It logs initialization, sets the
guard to one, and returns one.

## Caller Context

One data reference from `nppt_smac_init @ 0x129c8` populates this function in a
GEPHY initialization callback table. No direct `BL` caller exists.

## Concurrency and Ownership

The guard and four adjacent base slots are unprotected. Concurrent first calls
can interleave the base writes and initialization log.

## Evidence

- Complete ARM64 body at `0x1b340` through `0x1b39c`.
- No input register reads after prologue, proving the callback byte is unused.
- Guard `LDRB`/`STRB`, exact contiguous slot offsets, and base offsets.
- One data xref from SMAC initialization callback setup.
- SMAC assigns the entry to an `int (*)(u8)` callback slot; IDA records the
  recovered unused-byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Semantic ownership of the four APB subranges and caller synchronization.
