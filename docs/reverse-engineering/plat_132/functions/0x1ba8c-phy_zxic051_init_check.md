# 0x1ba8c phy_zxic051_init_check

## Status

- Status: complete
- Confidence: verified byte ABI, signed reader slot, PHY-ID lookup, both reads,
  re-init condition, return values, and direct caller.
- Size: `0x90` bytes, 35 ARM64 instructions.
- Recovered signature: `int phy_zxic051_init_check(u8 phy)`.

## Semantics

The helper computes a signed MDIO reader slot `phy - 4`, obtains a PHY ID, and
reads extended register 31 selectors 41 and 28. When the first value is zero
and the second is one, it calls `phy_zxic_051_phy_init(phy)` then returns `-1`.
All other value combinations return zero; the external init result is ignored.

## Caller Context

The sole direct caller, `phy_zxic051_para_init @ 0x1bb1c`, uses the result to
decide whether to run its parameter initialization write.

## Concurrency and Ownership

No local lock protects the signed callback table, PHY-ID mapping, or external
initialization sequence.

## Evidence

- Complete ARM64 body at `0x1ba8c` through `0x1bb18`.
- Exact signed slot arithmetic, selector values 41/28, `CCMP` compound
  condition, external init call, and zero/-1 return paths.
- Exhaustive direct xref query found only `phy_zxic051_para_init`.
- IDA type at `0x1ba8c` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of register-31 selector values 41 and 28.
