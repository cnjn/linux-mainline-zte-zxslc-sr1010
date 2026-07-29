# 0x18ec4 xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en

## Status

- Status: complete
- Confidence: verified selector/address branches, byte input, bit-13 RMW,
  void return, and all direct callers.
- Size: `0x80` bytes, 32 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en(u8 xmac, u8 enable)`.

## Semantics

The function truncates both inputs to bytes, selects the PCS word at offset
`0x0e0000`, reads it, clears bit 13, and inserts enable bit zero:

```c
control = (control & 0xffffdfffU) | ((enable & 1U) << 13);
```

Selectors two and three use raw address `0x0e0000 + (xmac << 23)`. Other
selectors use `xmac0_pcs_base + 0x0e0000 + sign_extend32(xmac << 24)`.

## Caller Context

Four direct callers use the helper: HSGMII exit, USXGMII exit, HSGMII
configuration, and USXGMII configuration. No caller consumes its incidental
pointer return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 32-instruction ARM64 body at `0x18ec4` through `0x18f40`.
- Exact byte truncations, `UBFIZ W1,W1,#13,#1`, and `0xffffdfff` mask.
- Both direct and `xmac0_pcs_base`-relative pointer paths.
- Exhaustive direct xref query found four caller sites.
- IDA type at `0x18ec4` updated to the recovered void byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS semantics of the vendor XS/PCS VSMMD1 control label.
