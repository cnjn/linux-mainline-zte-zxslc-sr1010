# 0x17280 xmac_5gbase_r_conf

## Status

- Status: complete
- Confidence: verified selector validation/truncation, CPU-specific call order,
  return aggregation, bypass predicate, work-mode write, and all three callers.
- Size: `0xf8` bytes, 60 ARM64 instructions.
- Recovered signature: `int xmac_5gbase_r_conf(u8 xmac)`.

## Semantics

The byte selector must be at most four. Larger values log `xmac_id(%d) is error` and return `-1` without modifying TX/RX state or the work-mode cache.

Valid selectors first disable XMAC TX and RX. The remaining sequence mirrors the
10GBASE-R setter with distinct raw parameters:

| Predicate | Ordered actions | Returned status |
| --- | --- | --- |
| `xmac <= 1 && isCpuType_132() == 1` | `xpcs_5gbase_r_conf`, `xamc_init_conf_by_speed(xmac, 5)`, `__const_udelay(0x8312b0)`, `uni_serdes_init(xmac, 2)` | XPCS result OR SerDes result |
| all other valid selectors | `uni_serdes_init(xmac, 2)`, `__const_udelay(0x8312b0)`, `xpcs_5gbase_r_conf`, `xamc_init_conf_by_speed(xmac, 5)` | XPCS result only |

In the second row, CPU 133 with selector zero or one additionally calls
`byPassEnableSet(xmac, 1)`. The SerDes result there is discarded. Both valid
paths write `sg_xmac_work_mode[xmac] = 1`, including paths that return nonzero
setup status.

## Caller Context

There are three direct callers: `xmac_init_by_work_mode @ 0x17da0`,
`xmac_test_siwtch_work_mode @ 0x17a50`, and `xmac_mode_set @ 0x17bd8`.

## Concurrency and Ownership

No local lock, allocation, or cleanup. TX/RX, SerDes, XPCS, and bypass effects
are delegated to helpers; the only local persistent write is the work-mode
cache after a valid selector path.

## Evidence

- Complete 60-instruction ARM64 body at `0x17280` through `0x17374`.
- Exact CPU-132 branch, SerDes mode two, speed argument five, and shared delay
  literal.
- Status OR exists only in the CPU-132 selector-0/1 branch.
- CPU-133 bypass predicate, unconditional valid-path mode-one cache store, and
  three caller xrefs.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of SerDes mode two and speed value five.
- Why CPU 132 selects a different call order for XMAC0/1.
- Side-effect/error contracts of the delegated helpers.
