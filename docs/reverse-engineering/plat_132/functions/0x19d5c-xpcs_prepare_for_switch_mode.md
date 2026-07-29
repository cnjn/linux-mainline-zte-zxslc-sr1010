# 0x19d5c xpcs_prepare_for_switch_mode

## Status

- Status: complete
- Confidence: verified two-argument ABI, auto-flag clear, comparison gate,
  mode dispatch, discarded exit returns, void return, and all direct callers.
- Size: `0x70` bytes, 27 ARM64 instructions.
- Recovered signature:
  `void xpcs_prepare_for_switch_mode(u8 xmac, u32 target_mode)`.

## Semantics

The helper truncates `xmac` to a byte, always clears
`g_xmac_work_in_auto[xmac]`, then reads `sg_xpcs_mode[xmac]`. When the cached
mode equals `target_mode`, it returns immediately. Otherwise it dispatches only
these cached modes:

| Cached mode | Action |
| --- | --- |
| 3 | `xpcs_exit_sgmii_mode(xmac)` |
| 5, 6, 7 | `xpcs_exit_usxgmii_mode(xmac)` |
| 8 | `xpcs_exit_hsgmii_mode(xmac)` |

All other cached modes return without an exit call. The helper ignores every
exit helper return value and does not update `sg_xpcs_mode[xmac]` itself.

## Caller Context

Seven PCS configuration paths call this preamble: 10GBASE-R, 5GBASE-R,
2.5GBASE-X, 1000BASE-X, HSGMII, SGMII, and USXGMII. Each passes the mode it is
about to configure.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The shared auto-mode
byte is cleared without synchronization before any selected PCS exit call.

## Evidence

- Complete 27-instruction ARM64 body at `0x19d5c` through `0x19dc8`.
- Exact byte truncation and unconditional zero-byte store at `0x19d70`.
- Cached-mode equality gate and complete jump-table dispatch.
- Exhaustive direct xref query found seven configuration callers.
- IDA type at `0x19d5c` updated to the recovered void two-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Why cached mode four has no explicit exit case.
- Why the vendor ignores every selected exit helper's return status.
