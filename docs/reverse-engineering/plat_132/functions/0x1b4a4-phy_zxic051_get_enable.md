# 0x1b4a4 phy_zxic051_get_enable

## Status

- Status: complete
- Confidence: verified no-argument ABI, direct tail-result propagation, and PHY
  callback-table reference.
- Size: `0x14` bytes, 5 ARM64 instructions.
- Recovered signature: `int phy_zxic051_get_enable(void)`.

## Semantics

The wrapper has no parameters and returns the direct result of the external
`phy_zxic_051_get_enable()` call.

## Caller Context

One data reference from `xmac_zxic_phy_init @ 0x18348` populates this helper in
a PHY callback table. No direct `BL` caller exists.

## Concurrency and Ownership

All state, synchronization, and error behavior is delegated to the external
getter.

## Evidence

- Complete ARM64 body at `0x1b4a4` through `0x1b4b4`.
- No argument setup before the sole call and no instruction alters W0 afterward.
- One data xref from the XMAC ZXIC PHY callback setup.
- IDA type at `0x1b4a4` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Return-value contract of the external getter.
