# 0x1bb1c phy_zxic051_para_init

## Status

- Status: complete
- Confidence: verified byte ABI, discarded first lookup, propagated second call
  result, and sole direct caller.
- Size: `0x2c` bytes, 11 ARM64 instructions.
- Recovered signature: `int phy_zxic051_para_init(u8 phy)`.

## Semantics

The helper truncates `phy` to a byte, calls `phy_051_g_phy_id_check(phy)` and
discards its result, then returns the direct result of `phy_zxic051_init_check`.

## Caller Context

Its sole direct caller is `xmac_zxic_phy_init @ 0x18348`.

## Concurrency and Ownership

No local state, locking, allocation, or cleanup exists; all effects are
delegated to the two calls.

## Evidence

- Complete ARM64 body at `0x1bb1c` through `0x1bb44`.
- Preserved byte input across the discarded first call and direct W0 result from
  the second call.
- Exhaustive direct xref query found only `xmac_zxic_phy_init`.
- IDA type at `0x1bb1c` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Whether the discarded first lookup has cache, state, or transport effects.
