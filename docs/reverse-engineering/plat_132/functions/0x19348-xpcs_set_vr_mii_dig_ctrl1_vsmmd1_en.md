# 0x19348 xpcs_set_vr_mii_dig_ctrl1_vsmmd1_en

## Status

- Status: complete
- Confidence: verified selector/address branches, byte input, bit-13 RMW,
  void return, and both direct callers.
- Size: `0x80` bytes, 32 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_vr_mii_dig_ctrl1_vsmmd1_en(u8 xmac, u8 enable)`.

## Semantics

The function truncates both inputs to bytes, selects the PCS word at offset
`0x7e0000`, reads it, clears bit 13, and inserts enable bit zero:

```c
control = (control & 0xffffdfffU) | ((enable & 1U) << 13);
```

Selectors two and three use raw address `0x7e0000 + (xmac << 23)`. Other
selectors use `xmac0_pcs_base + 0x7e0000 + sign_extend32(xmac << 24)`.

## Caller Context

`xpcs_exit_hsgmii_mode` clears this control during a transition from cached mode
eight. `xpcs_hsgmii_mode_conf` sets it during HSGMII configuration. Neither
caller consumes its incidental pointer return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 32-instruction ARM64 body at `0x19348` through `0x193c4`.
- Exact byte truncations, `UBFIZ W1,W1,#13,#1`, and `0xffffdfff` mask.
- Both direct and `xmac0_pcs_base`-relative pointer paths.
- Exhaustive direct xref query found two caller sites.
- IDA type at `0x19348` updated to the recovered void byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS semantics of the vendor VSMMD1 control label.
