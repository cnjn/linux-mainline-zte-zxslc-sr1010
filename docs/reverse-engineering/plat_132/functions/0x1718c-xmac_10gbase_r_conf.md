# 0x1718c xmac_10gbase_r_conf

## Status

- Status: complete
- Confidence: verified selector validation/truncation, CPU-specific call order,
  return aggregation, bypass predicate, work-mode write, and all three callers.
- Size: `0xf4` bytes, 58 ARM64 instructions.
- Recovered signature: `int xmac_10gbase_r_conf(u8 xmac)`.

## Semantics

The selector is a byte and must be at most four. Larger values log
`xmac_id(%d) is error` and return `-1` without touching TX/RX state or the
work-mode cache.

For a valid selector, the function first disables XMAC TX and RX. Its remaining
sequence is CPU and selector dependent:

| Predicate | Ordered actions | Returned status |
| --- | --- | --- |
| `xmac <= 1 && isCpuType_132() == 1` | `xpcs_10gbase_r_conf`, `xamc_init_conf_by_speed(xmac, 0)`, `__const_udelay(0x8312b0)`, `uni_serdes_init(xmac, 0)` | XPCS result OR SerDes result |
| all other valid selectors | `uni_serdes_init(xmac, 0)`, `__const_udelay(0x8312b0)`, `xpcs_10gbase_r_conf`, `xamc_init_conf_by_speed(xmac, 0)` | XPCS result only |

In the second row, CPU 133 with selector zero or one additionally calls
`byPassEnableSet(xmac, 1)`. The SerDes result in that path is explicitly
discarded. Both valid paths write `sg_xmac_work_mode[xmac] = 0` even when the
returned status is nonzero.

## Caller Context

There are three direct callers: the mode dispatcher
`xmac_init_by_work_mode @ 0x17da0`, test-mode switch helper
`xmac_test_siwtch_work_mode @ 0x17a50`, and `xmac_mode_set @ 0x17bd8`.

## Concurrency and Ownership

No local lock, allocation, or cleanup. TX/RX state and SerDes/XPCS programming
are delegated to helpers. The function directly changes only the XMAC
work-mode cache after valid-path setup.

## Evidence

- Complete 58-instruction ARM64 body at `0x1718c` through `0x1727c`.
- `UXTB`, unsigned selector limit, and invalid `0xffffffff` return.
- Exact branch-specific helper order, delay literal, status OR, and CPU-133
  bypass call.
- Unconditional zero-mode cache store after both valid branches.
- Three direct caller xrefs.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware interpretation of bypass enable and both raw delay values.
- Why CPU 132 treats selectors zero/one with a different initialization order.
- Side-effect/error contracts of the delegated XPCS and SerDes helpers.
