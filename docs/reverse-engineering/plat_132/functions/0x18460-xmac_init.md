# 0x18460 xmac_init

## Status

- Status: complete
- Confidence: verified PHY-type branch, exact helper order, independent XMAC
  mode gates, status OR, diagnostics, direct caller, and return value.
- Size: `0xd0` bytes, 52 ARM64 instructions.
- Recovered signature: `int xmac_init(u32 xmac0_work_mode, u32 xmac1_work_mode)`.

## Semantics

The function first calls imported `xmac_phy_type()`. Raw type value nine skips
the PHY setup sequence. Every other value invokes these helpers in order and
ignores their raw return registers:

1. `phy_dynamic_identify()`
2. `xmac_rlt_phy_init()`
3. `xmac_mvl_phy_init()`
4. `xmac_aqr_phy_init()`
5. `xmac_zxic_phy_init()`

It initializes `status` to zero. If `xmac_need_set_work_mode` is nonzero, it
sets `status` to `xmac_init_by_work_mode(0, xmac0_work_mode)`. Independently,
if raw global `dword_2677C` is nonzero, it ORs
`xmac_init_by_work_mode(1, xmac1_work_mode)` into `status`. A first-mode failure
does not prevent the enabled second-mode call.

A nonzero aggregate logs both requested modes. A nonzero
`g_ponserdes_to_xmac1` only emits the XMAC1-mode diagnostic and does not alter
control flow or the returned status. The function returns the 32-bit OR result.

## Caller Context

`nppt_smac_init @ 0x129c8` is the sole direct in-module caller. That caller
chooses the mode pair from its raw PHY-type table but deliberately ignores this
function's returned status before continuing to worker startup.

## Concurrency and Ownership

No local lock, allocation, direct MMIO access, or cleanup occurs. All PHY
registration and XMAC hardware effects are delegated to the five setup helpers
and `xmac_init_by_work_mode`.

## Evidence

- Complete 52-instruction ARM64 body at `0x18460` through `0x1852c`.
- `CMP W0, #9` directs exactly the five-call PHY setup sequence.
- Independent `CBZ` gates at `0x18498` and `0x184b4`, and `ORR W19, W19, W0`
  establish the status aggregation and lack of first-error short circuit.
- Sole caller xref and direct analysis of all five PHY setup helpers.
- Runtime dmesg records the resulting XMAC work modes five and four on CPU 133.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware/product meaning of raw PHY type nine and both mode-gate globals.
- Detailed XMAC work-mode programming inside `xmac_init_by_work_mode`.
- Whether any delegated PHY helper has an error contract intentionally ignored
  by this coordinator.
