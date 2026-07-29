# 0x175b0 xmac_5g_usxgmii_auto_conf

## Status

- Status: complete
- Confidence: verified selector validation/truncation, CPU-specific call order,
  PCS/auto-negotiation arguments, status aggregation, bypass predicate, cache
  write, and all three callers.
- Size: `0x12c` bytes, 73 ARM64 instructions.
- Recovered signature: `int xmac_5g_usxgmii_auto_conf(u8 xmac)`.

## Semantics

The byte selector must be at most four; invalid values log `xmac_id(%d) is error` and return `-1` without modifying TX/RX state or the work-mode cache.

Valid selectors disable TX/RX. Both forms use USXGMII mode one,
auto-negotiation value one, MAC speed five, and SerDes mode three:

| Predicate | Ordered actions after TX/RX disable | Returned status |
| --- | --- | --- |
| `xmac <= 1 && isCpuType_132() == 1` | PCS mode, PCS auto-negotiation, `xamc_init_conf_by_speed(xmac, 5)`, `__const_udelay(0x8312b0)`, `uni_serdes_init(xmac, 3)` | PCS mode OR auto-negotiation OR SerDes |
| all other valid selectors | `uni_serdes_init(xmac, 3)`, `__const_udelay(0x8312b0)`, PCS mode, PCS auto-negotiation, `xamc_init_conf_by_speed(xmac, 5)` | PCS mode OR auto-negotiation |

In the second row, only CPU 133 selector zero or one calls
`byPassEnableSet(xmac, 1)`. CPU 129 does not take this bypass path. Every valid
path writes `sg_xmac_work_mode[xmac] = 6`, including nonzero status paths.

## Caller Context

There are three direct callers: `xmac_init_by_work_mode @ 0x17da0`,
`xmac_test_siwtch_work_mode @ 0x17a50`, and `xmac_mode_set @ 0x17bd8`.

## Concurrency and Ownership

No local lock, allocation, or cleanup. TX/RX, PCS, SerDes, auto-negotiation,
and bypass behavior are delegated to helpers; only the work-mode cache is
directly persisted on valid paths.

## Evidence

- Complete 73-instruction ARM64 body at `0x175b0` through `0x176d8`.
- Exact USXGMII mode one, auto-negotiation one, speed five, and SerDes mode three
  arguments.
- CPU-132 three-result OR versus non-132 two-result OR and CPU-133-only bypass.
- Valid-path mode-six cache store and three caller xrefs.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of USXGMII mode one and SerDes mode three.
- Why CPU 129 is excluded from bypass for USXGMII auto modes.
- Side-effect/error contracts of delegated PCS and SerDes helpers.
