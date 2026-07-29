# 0x17378 xmac_2pt5gbase_x_conf

## Status

- Status: complete
- Confidence: verified selector validation/truncation, CPU-specific call order,
  status aggregation, bypass predicate, cache write, and all four callers.
- Size: `0x10c` bytes, 65 ARM64 instructions.
- Recovered signature: `int xmac_2pt5gbase_x_conf(u8 xmac)`.

## Semantics

The byte selector must be at most four; invalid values log `xmac_id(%d) is error` and return `-1` without modifying TX/RX state or the work-mode cache.

Valid selectors first disable XMAC TX and RX. Their branches are:

| Predicate | Ordered actions | Returned status |
| --- | --- | --- |
| `xmac <= 1 && isCpuType_132() == 1` | `xpcs_2p5gbase_x_conf`, `xamc_init_conf_by_speed(xmac, 6)`, `__const_udelay(0x8312b0)`, `uni_serdes_init(xmac, 5)` | XPCS result OR SerDes result |
| all other valid selectors | `uni_serdes_init(xmac, 5)`, `__const_udelay(0x8312b0)`, `xpcs_2p5gbase_x_conf`, `xamc_init_conf_by_speed(xmac, 6)` | XPCS result only |

In the second row, CPU 133 or 129 with selector zero or one additionally calls
`byPassEnableSet(xmac, 1)`. The SerDes return in that branch is discarded. Every
valid path writes `sg_xmac_work_mode[xmac] = 4`, including nonzero status paths.

## Caller Context

There are four direct callers: `xmac_init_by_work_mode @ 0x17da0`,
`xmac_test_siwtch_work_mode @ 0x17a50`, `xmac_mode_set @ 0x17bd8`, and
`phy_051_set_xmac_work_mode @ 0x1bfa4`.

## Concurrency and Ownership

No local lock, allocation, or cleanup. TX/RX, XPCS, SerDes, and bypass behavior
are delegated to helpers; the only direct persistent write is the valid-path
work-mode cache update.

## Evidence

- Complete 65-instruction ARM64 body at `0x17378` through `0x17480`.
- CPU-132 status OR, XPCS 2.5GBASE-X, speed value six, SerDes mode five, and
  raw delay literal.
- CPU-133-or-129 bypass predicate, mode-four cache store, and four caller xrefs.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of speed six and SerDes mode five.
- Why CPU 129 participates in the bypass path.
- Side-effect/error contracts of delegated XPCS and SerDes helpers.
