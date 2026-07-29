# 0x1ad04 zte_gephy_get_short_reach_en

## Status

- Status: complete
- Confidence: verified null handling, selector truncation, APB-base lookup,
  empty-slot behavior, offset/bit extraction, and no direct xrefs.
- Size: `0x54` bytes, 20 ARM64 instructions.
- Recovered signature: `int zte_gephy_get_short_reach_en(u8 phy, u8 *enable)`.

## Semantics

The function rejects a null output pointer, logs `"eee_en_status is null\n"`,
and returns `-1`. With a valid pointer it uses the byte-truncated PHY index to
read `sg_zxicgephy_apb_base[phy]`. A zero slot returns zero without writing the
output. A nonzero slot reads the 32-bit word at APB offset `0x90`, stores bit
zero to the output byte, and returns zero. It does not bounds-check the index.

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY control path; direct xrefs cannot establish that.

## Concurrency and Ownership

The caller owns output storage and APB-base slot lifetime. There is no locking
or mapping validation in this helper.

## Evidence

- Complete ARM64 body at `0x1ad04` through `0x1ad54`.
- Null output path, exact reused log string, and `-1` return.
- Byte index, eight-byte pointer-array stride, empty-slot zero return, and
  offset `0x90` bit-zero read.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1ad04` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- APB offset `0x90` hardware semantics and the expected array bounds.
