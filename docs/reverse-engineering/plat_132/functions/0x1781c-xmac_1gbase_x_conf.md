# 0x1781c xmac_1gbase_x_conf

## Status

- Status: complete
- Confidence: verified selector validation/truncation, CPU-specific call order,
  XPCS arguments, status aggregation, bypass predicate, cache write, and all
  three callers.
- Size: `0x11c` bytes, 69 ARM64 instructions.
- Recovered signature: `int xmac_1gbase_x_conf(u8 xmac)`.

## Semantics

The byte selector must be at most four; larger values log `xmac_id(%d) is error` and return `-1` without modifying TX/RX state or the work-mode cache.

Valid selectors first disable XMAC TX and RX. The two valid branches are:

| Predicate | Ordered actions | Returned status |
| --- | --- | --- |
| `xmac <= 1 && isCpuType_132() == 1` | `xpcs_1000base_x_conf(xmac, 3, 1)`, `xamc_init_conf_by_speed(xmac, 3)`, `__const_udelay(0x8312b0)`, `uni_serdes_init(xmac, 7)` | XPCS result OR SerDes result |
| all other valid selectors | `uni_serdes_init(xmac, 7)`, `__const_udelay(0x8312b0)`, `xpcs_1000base_x_conf(xmac, 3, 1)`, `xamc_init_conf_by_speed(xmac, 3)` | XPCS result only |

In the second row, selectors zero or one call `byPassEnableSet(xmac, 1)` when
CPU type is 133 or 129. The SerDes return in that branch is discarded. Both
valid paths write `sg_xmac_work_mode[xmac] = 2`, even for a nonzero setup
status.

## Caller Context

There are three direct callers: `xmac_init_by_work_mode @ 0x17da0`,
`xmac_test_siwtch_work_mode @ 0x17a50`, and `xmac_mode_set @ 0x17bd8`.

## Concurrency and Ownership

No local lock, allocation, or cleanup. TX/RX, XPCS, SerDes, and bypass effects
are delegated to helpers; the only direct persistent write is the valid-path
work-mode cache update.

## Evidence

- Complete 69-instruction ARM64 body at `0x1781c` through `0x17934`.
- Exact `xpcs_1000base_x_conf(xmac, 3, 1)` argument registers in both branches.
- CPU-132 status OR, SerDes mode seven, MAC speed value three, and raw delay.
- CPU-133-or-129 bypass predicate, mode-two cache store, and three caller xrefs.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Semantics of the XPCS third argument and SerDes mode seven.
- Why CPU 129 participates in this bypass condition but not 5GBASE-R.
- Side-effect/error contracts of the delegated helpers.
