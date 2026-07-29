# 0x17938 xmac_hsgmii_conf

## Status

- Status: complete
- Confidence: verified both byte truncations, CPU-specific call order, variant
  forwarding, status aggregation, bypass predicate, cache write, and all four
  caller xrefs.
- Size: `0x118` bytes, 68 ARM64 instructions.
- Recovered signature: `int xmac_hsgmii_conf(u8 xmac, u8 variant)`.

## Semantics

The XMAC selector must be at most four; invalid selectors log
`xmac_id(%d) is error` and return `-1` without modifying TX/RX state or the
work-mode cache. The variant input is truncated to a byte before forwarding to
the HSGMII PCS helper.

Valid selectors disable TX/RX and follow one of these paths:

| Predicate | Ordered actions | Returned status |
| --- | --- | --- |
| `xmac <= 1 && isCpuType_132() == 1` | `xpcs_hsgmii_mode_conf(xmac, variant)`, `xamc_init_conf_by_speed(xmac, 2)`, `__const_udelay(0x8312b0)`, `uni_serdes_init(xmac, 5)` | PCS result OR SerDes result |
| all other valid selectors | `uni_serdes_init(xmac, 5)`, `__const_udelay(0x8312b0)`, `xpcs_hsgmii_mode_conf(xmac, variant)`, `xamc_init_conf_by_speed(xmac, 2)` | PCS result only |

In the second row, CPU 133 or 129 with selector zero or one additionally calls
`byPassEnableSet(xmac, 1)`. The SerDes result in that path is discarded. Every
valid path writes `sg_xmac_work_mode[xmac] = 8`, including nonzero status paths.

## Caller Context

There are four direct caller xrefs: `xmac_init_by_work_mode @ 0x17da0`,
`xmac_mode_set @ 0x17bd8`, and two sites in
`xmac_test_siwtch_work_mode @ 0x17a50`. The dispatcher uses variants one for
mode eight and zero for mode nine.

## Concurrency and Ownership

No local lock, allocation, or cleanup. TX/RX, PCS, SerDes, and bypass behavior
are delegated to helpers; only the work-mode cache is directly persisted on
valid paths.

## Evidence

- Complete 68-instruction ARM64 body at `0x17938` through `0x17a4c`.
- Exact `UXTB` handling for both inputs and variant forwarding to HSGMII PCS.
- CPU-132 status OR versus non-132 PCS-only result, shared delay, and SerDes
  mode five.
- CPU-133-or-129 bypass predicate, mode-eight cache store, and four caller xrefs.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of HSGMII variants zero and one.
- Why the cache stores mode eight for both variant paths.
- Side-effect/error contracts of delegated PCS and SerDes helpers.
