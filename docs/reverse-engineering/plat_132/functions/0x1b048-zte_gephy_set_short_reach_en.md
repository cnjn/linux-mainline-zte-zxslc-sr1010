# 0x1b048 zte_gephy_set_short_reach_en

## Status

- Status: complete
- Confidence: verified selector truncation, APB-base lookup, empty-slot path,
  `apb_bit_write` arguments, zero return, and no direct xrefs.
- Size: `0x44` bytes, 17 ARM64 instructions.
- Recovered signature: `int zte_gephy_set_short_reach_en(u8 phy, u8 enable)`.

## Semantics

The helper uses the byte-truncated PHY index to read
`sg_zxicgephy_apb_base[phy]`. When the slot is nonzero it calls
`apb_bit_write(base + 0x90, enable, 1, 0)`. A zero slot skips the write. Every
path returns zero, and no index bounds check exists.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY control path; direct xrefs cannot establish that.

## Concurrency and Ownership

The caller owns APB-base slot lifetime. No local locking or mapping validation
exists around the delegated bit write.

## Evidence

- Complete ARM64 body at `0x1b048` through `0x1b088`.
- Byte index, eight-byte pointer-array stride, and empty-slot zero return.
- Exact helper call arguments: APB offset `0x90`, raw byte value, width one,
  and bit offset zero.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1b048` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- `apb_bit_write` handling of raw enable values above one.
