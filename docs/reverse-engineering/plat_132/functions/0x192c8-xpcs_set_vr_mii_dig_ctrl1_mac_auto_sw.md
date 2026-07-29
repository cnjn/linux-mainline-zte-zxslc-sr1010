# 0x192c8 xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw

## Status

- Status: complete
- Confidence: verified selector/address branches, byte input, bit-nine RMW,
  void return, and all direct callers.
- Size: `0x80` bytes, 32 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw(u8 xmac, u8 enable)`.

## Semantics

The function truncates both inputs to bytes, selects the PCS word at offset
`0x7e0000`, reads it, clears bit nine, and inserts enable bit zero at bit nine:

```c
control = (control & 0xfffffdffU) | ((enable & 1U) << 9);
```

Selectors two and three use raw address `0x7e0000 + (xmac << 23)`. Other
selectors use `xmac0_pcs_base + 0x7e0000 + sign_extend32(xmac << 24)`.

## Caller Context

Four direct callers use the helper: SGMII auto-negotiation configuration,
1000BASE-X auto-negotiation configuration, SGMII exit, and HSGMII configuration.
No caller consumes its incidental pointer return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 32-instruction ARM64 body at `0x192c8` through `0x19344`.
- Exact byte truncations, `UBFIZ W1,W1,#9,#1`, and `0xfffffdff` mask.
- Both direct and `xmac0_pcs_base`-relative pointer paths.
- Exhaustive direct xref query found four caller sites.
- IDA type at `0x192c8` updated to the recovered void byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact physical semantics of the vendor MAC-auto-switch control bit.
