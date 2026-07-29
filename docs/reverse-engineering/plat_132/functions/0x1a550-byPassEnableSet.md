# 0x1a550 byPassEnableSet

## Status

- Status: complete
- Confidence: verified CPU gates, selector/address branches, nonzero-enable
  predicate, bit-four RMW, zero return, and all direct callers.
- Size: `0xb8` bytes, 44 ARM64 instructions.
- Recovered signature: `int byPassEnableSet(u8 xmac, u8 enable)`.

## Semantics

The helper first checks whether CPU type 133 equals one, then CPU type 129 if
needed. On any other CPU it returns zero without MMIO. On either accepted CPU,
it reads selector-specific PCS offset `0x0e0014` and sets bit four when `enable`
is nonzero or clears it when `enable` is zero; it then returns zero.

Selectors two and three use the raw `0x0e0014 + (xmac << 23)` window. Other
selectors use `xmac0_pcs_base + 0x0e0014 + sign_extend32(xmac << 24)`. There is
no selector-range check.

## Caller Context

Nine direct callers in the SGMII, 10GBASE-R, 5GBASE-R, 2.5GBASE-X, USXGMII,
1000BASE-X, and HSGMII XMAC configuration paths pass a nonzero enable value.
They do not use the always-zero status.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete ARM64 body at `0x1a550` through `0x1a604`.
- Ordered CPU-133 then CPU-129 predicates, each compared with literal one.
- `CSEL` selects `(value | 0x10)` for any nonzero enable and
  `(value & 0xffffffef)` only for zero.
- Exhaustive direct xref query found nine XMAC mode-configuration callers.
- IDA type at `0x1a550` updated to the recovered integer-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware semantics of the CPU-gated PCS bypass bit.
