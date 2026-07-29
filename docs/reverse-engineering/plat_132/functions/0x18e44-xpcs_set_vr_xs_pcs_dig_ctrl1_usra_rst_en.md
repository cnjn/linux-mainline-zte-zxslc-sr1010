# 0x18e44 xpcs_set_vr_xs_pcs_dig_ctrl1_usra_rst_en

## Status

- Status: complete
- Confidence: verified selector/address branches, byte input, bit-10 RMW, void
  return, and both direct callers.
- Size: `0x80` bytes, 32 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_vr_xs_pcs_dig_ctrl1_usra_rst_en(u8 xmac, u8 enable)`.

## Semantics

The function truncates both inputs to bytes, selects the PCS word at offset
`0x0e0000`, reads it, clears bit 10, and inserts enable bit zero:

```c
control = (control & 0xfffffbffU) | ((enable & 1U) << 10);
```

Selectors two and three use raw address `0x0e0000 + (xmac << 23)`. Other
selectors use `xmac0_pcs_base + 0x0e0000 + sign_extend32(xmac << 24)`.

## Caller Context

The USXGMII auto-disable and auto-enable speed/duplex paths each request this
control with literal one immediately before they return the VR-reset wait status.
Neither caller consumes its incidental pointer return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 32-instruction ARM64 body at `0x18e44` through `0x18ec0`.
- Exact byte truncations, `UBFIZ W1,W1,#10,#1`, and `0xfffffbff` mask.
- Both direct and `xmac0_pcs_base`-relative pointer paths.
- Exhaustive direct xref query found two caller sites.
- IDA type at `0x18e44` updated to the recovered void byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS role and reset relationship of the vendor USRA control label.
