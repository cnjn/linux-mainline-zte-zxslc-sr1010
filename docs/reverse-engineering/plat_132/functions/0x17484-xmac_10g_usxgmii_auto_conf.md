# 0x17484 xmac_10g_usxgmii_auto_conf

## Status

- Status: complete
- Confidence: verified selector validation/truncation, CPU-specific call order,
  PCS/auto-negotiation arguments, status aggregation, bypass predicate, cache
  write, and all three callers.
- Size: `0x12c` bytes, 73 ARM64 instructions.
- Recovered signature: `int xmac_10g_usxgmii_auto_conf(u8 xmac)`.

## Semantics

The byte selector must be at most four; invalid values log `xmac_id(%d) is error` and return `-1` without modifying TX/RX state or the work-mode cache.

For valid selectors, TX and RX are disabled. Both forms call
`xpcs_usxgmii_mode_conf(xmac, 0)` and
`xpcs_auto_negotiation_conf_in_usxgmii_mode(xmac, 1)`, then configure MAC speed
zero and use SerDes mode one:

| Predicate | Ordered actions after TX/RX disable | Returned status |
| --- | --- | --- |
| `xmac <= 1 && isCpuType_132() == 1` | PCS mode, PCS auto-negotiation, `xamc_init_conf_by_speed(xmac, 0)`, `__const_udelay(0x8312b0)`, `uni_serdes_init(xmac, 1)` | PCS mode OR auto-negotiation OR SerDes |
| all other valid selectors | `uni_serdes_init(xmac, 1)`, `__const_udelay(0x8312b0)`, PCS mode, PCS auto-negotiation, `xamc_init_conf_by_speed(xmac, 0)` | PCS mode OR auto-negotiation |

In the second row, CPU 133 with selector zero or one additionally calls
`byPassEnableSet(xmac, 1)`. CPU 129 does not take this bypass path. Every valid
path writes `sg_xmac_work_mode[xmac] = 5`, including nonzero status paths.

## Caller Context

There are three direct callers: `xmac_init_by_work_mode @ 0x17da0`,
`xmac_test_siwtch_work_mode @ 0x17a50`, and `xmac_mode_set @ 0x17bd8`.

## Concurrency and Ownership

No local lock, allocation, or cleanup. TX/RX, PCS, SerDes, auto-negotiation,
and bypass behavior are delegated to helpers; this function directly persists
only the valid-path work-mode cache.

## Evidence

- Complete 73-instruction ARM64 body at `0x17484` through `0x175ac`.
- Exact PCS mode zero, auto-negotiation one, MAC speed zero, and SerDes mode one
  arguments in both CPU branches.
- CPU-132 three-result OR versus non-132 two-result OR and CPU-133-only bypass.
- Valid-path mode-five cache store and three caller xrefs.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of USXGMII mode zero, auto-negotiation one, and SerDes mode
  one.
- Why CPU 129 is excluded from this bypass path.
- Side-effect/error contracts of delegated PCS and SerDes helpers.
