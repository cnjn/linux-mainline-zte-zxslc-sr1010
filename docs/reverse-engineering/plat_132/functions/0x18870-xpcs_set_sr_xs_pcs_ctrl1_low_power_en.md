# 0x18870 xpcs_set_sr_xs_pcs_ctrl1_low_power_en

## Status

- Status: complete
- Confidence: verified selector/address branches, byte input, bit-11 RMW, void
  return, and all direct callers.
- Size: `0x80` bytes, 32 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_sr_xs_pcs_ctrl1_low_power_en(u8 xmac, u8 enable)`.

## Semantics

The function truncates both inputs to bytes, selects the PCS word at offset
`0x0c0000`, reads it, clears bit 11, and inserts enable bit zero:

```c
control = (control & 0xfffff7ffU) | ((enable & 1U) << 11);
```

Selectors two and three use raw address `0x0c0000 + (xmac << 23)`. Other
selectors use `xmac0_pcs_base + 0x0c0000 + sign_extend32(xmac << 24)`.

## Caller Context

Eight direct calls occur in the recovered 1G sequence and 10GBASE-R, 5GBASE-R,
and HSGMII PCS configuration paths. They use it around PSEQ wait operations to
enable or clear low-power state. No caller uses its incidental pointer return.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 32-instruction ARM64 body at `0x18870` through `0x188ec`.
- Exact byte truncations, `UBFIZ W1,W1,#11,#1`, and `0xfffff7ff` mask.
- Both direct and `xmac0_pcs_base`-relative pointer paths.
- Exhaustive direct xref query found eight caller sites.
- IDA type at `0x18870` updated to the recovered void byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS semantics of the SR-XS/PCS low-power control bit.
