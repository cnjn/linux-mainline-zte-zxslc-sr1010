# 0x176dc xmac_2pt5g_usxgmii_auto_conf

## Status

- Status: complete
- Confidence: verified selector validation/truncation, CPU-specific call order,
  PCS/auto-negotiation arguments, status aggregation, bypass predicate, cache
  write, and all three callers.
- Size: `0x140` bytes, 78 ARM64 instructions.
- Recovered signature: `int xmac_2pt5g_usxgmii_auto_conf(u8 xmac)`.

## Semantics

The byte selector must be at most four; invalid values log `xmac_id(%d) is error` and return `-1` without modifying TX/RX state or the work-mode cache.

Valid selectors disable TX/RX. Both forms use USXGMII mode two,
auto-negotiation value one, MAC speed six, and SerDes mode four:

| Predicate | Ordered actions after TX/RX disable | Returned status |
| --- | --- | --- |
| `xmac <= 1 && isCpuType_132() == 1` | PCS mode, PCS auto-negotiation, `xamc_init_conf_by_speed(xmac, 6)`, `__const_udelay(0x8312b0)`, `uni_serdes_init(xmac, 4)` | PCS mode OR auto-negotiation OR SerDes |
| all other valid selectors | `uni_serdes_init(xmac, 4)`, `__const_udelay(0x8312b0)`, PCS mode, PCS auto-negotiation, `xamc_init_conf_by_speed(xmac, 6)` | PCS mode OR auto-negotiation |

In the second row, CPU 133 or 129 with selector zero or one additionally calls
`byPassEnableSet(xmac, 1)`. Every valid path writes
`sg_xmac_work_mode[xmac] = 7`, including nonzero status paths.

## Caller Context

There are three direct callers: `xmac_init_by_work_mode @ 0x17da0`,
`xmac_test_siwtch_work_mode @ 0x17a50`, and `xmac_mode_set @ 0x17bd8`.

## Concurrency and Ownership

No local lock, allocation, or cleanup. TX/RX, PCS, SerDes, auto-negotiation,
and bypass behavior are delegated to helpers; only the work-mode cache is
directly persisted on valid paths.

## Evidence

- Complete 78-instruction ARM64 body at `0x176dc` through `0x17818`.
- Exact USXGMII mode two, auto-negotiation one, speed six, and SerDes mode four
  arguments.
- CPU-132 three-result OR versus non-132 two-result OR and CPU-133-or-129
  bypass predicate.
- Valid-path mode-seven cache store and three caller xrefs.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of USXGMII mode two and SerDes mode four.
- Why CPU 129 returns to the bypass condition for this auto mode.
- Side-effect/error contracts of delegated PCS and SerDes helpers.
