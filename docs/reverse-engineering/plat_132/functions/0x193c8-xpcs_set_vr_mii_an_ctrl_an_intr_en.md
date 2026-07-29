# 0x193c8 xpcs_set_vr_mii_an_ctrl_an_intr_en

## Status

- Status: complete
- Confidence: verified selector/address branches, byte input, bit-zero RMW,
  void return, and all direct callers.
- Size: `0x80` bytes, 31 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_vr_mii_an_ctrl_an_intr_en(u8 xmac, u8 enable)`.

## Semantics

The function truncates both inputs to bytes, selects the PCS word at offset
`0x7e0004`, reads it, clears bit zero, and inserts enable bit zero:

```c
control = (control & 0xfffffffeU) | (enable & 1U);
```

Selectors two and three use raw address `0x7e0004 + (xmac << 23)`. Other
selectors use `xmac0_pcs_base + 0x7e0004 + sign_extend32(xmac << 24)`.

## Caller Context

Five direct callers use this helper: USXGMII and SGMII auto-negotiation
configuration, 1000BASE-X auto-negotiation configuration, SGMII exit, and
HSGMII configuration. No caller consumes its incidental pointer return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 31-instruction ARM64 body at `0x193c8` through `0x19444`.
- Exact byte truncations, low-bit mask, and `0xfffffffe` RMW mask.
- Both direct and `xmac0_pcs_base`-relative pointer paths.
- Exhaustive direct xref query found five caller sites.
- IDA type at `0x193c8` updated to the recovered void byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS semantics of the vendor AN-interrupt-enable label and bit zero.
