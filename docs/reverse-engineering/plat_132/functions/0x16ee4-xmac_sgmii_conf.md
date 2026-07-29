# 0x16ee4 xmac_sgmii_conf

## Status

- Status: complete
- Confidence: verified selector/auto-negotiation byte truncation, all four
  forwarded arguments, CPU-specific order/status aggregation, bypass predicate,
  cache write, and all five caller xrefs.
- Size: `0x150` bytes, 82 ARM64 instructions.
- Recovered signature:
  `int xmac_sgmii_conf(u8 xmac, u8 auto_negotiation, u32 mode_value, u32 config_value)`.

## Semantics

The first selector is truncated to a byte and must be at most four. Invalid
selectors log `xmac_id(%d) is error` and return `-1` without changing TX/RX or
the work-mode cache. The second input is also truncated to a byte before use.

For valid selectors, TX and RX are disabled. Both branch forms call
`xpcs_sgmii_mode_conf(xmac, mode_value, config_value)` and
`xpcs_auto_negotiation_conf_in_sgmii_mode(xmac, auto_negotiation)`, with their
statuses ORed. Their order differs as follows:

| Predicate | Ordered actions after TX/RX disable | Returned status |
| --- | --- | --- |
| `xmac <= 1 && isCpuType_132() == 1` | PCS mode, PCS auto-negotiation, `xamc_init_conf_by_speed(xmac, 3)`, `__const_udelay(0x8312b0)`, `uni_serdes_init(xmac, 7)` | PCS mode OR auto-negotiation OR SerDes |
| all other valid selectors | `uni_serdes_init(xmac, 7)`, `__const_udelay(0x8312b0)`, PCS mode, PCS auto-negotiation, `xamc_init_conf_by_speed(xmac, 3)` | PCS mode OR auto-negotiation |

In the second row, CPU 133 or 129 with selector zero or one additionally calls
`byPassEnableSet(xmac, 1)`. Its SerDes return is discarded. All valid paths
write `sg_xmac_work_mode[xmac] = 3`, even after a nonzero aggregate status.

## Caller Context

There are five direct call sites: one in `xmac_init_by_work_mode @ 0x17da0`,
one in `xmac_test_siwtch_work_mode @ 0x17a50`, two in `xmac_mode_set @ 0x17bd8`,
and one in `phy_051_set_xmac_work_mode @ 0x1bfa4`. The dispatcher supplies the
fixed tuple `(0, 3, 1)` after the XMAC selector.

## Concurrency and Ownership

No local lock, allocation, or cleanup. TX/RX, PCS, SerDes, and bypass behavior
are delegated to helpers; this function directly persists only the work-mode
cache on valid paths.

## Evidence

- Complete 82-instruction ARM64 body at `0x16ee4` through `0x17030`.
- Exact `UXTB` handling of first and second arguments; W2/W3 forwarding to the
  PCS mode helper.
- CPU-132 three-result OR versus non-132 two-result OR, SerDes mode seven, and
  delay literal.
- CPU-133-or-129 bypass predicate, valid-path mode-three cache store, and five
  caller xrefs.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of both raw PCS configuration inputs and auto-negotiation
  byte.
- Why direct PHY and test-mode callers supply different argument tuples.
- Side-effect/error contracts of the delegated PCS and SerDes helpers.
