# 0x18c68 xpcs_set_vr_xs_pcs_xaui_ctrl_xaui_mode.constprop.1

## Status

- Status: complete
- Confidence: verified selector/address branches, bit-zero RMW, void return,
  and all direct callers.
- Size: `0x74` bytes, 28 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_vr_xs_pcs_xaui_ctrl_xaui_mode_constprop_1(u8 xmac)`.

## Semantics

The function truncates the selector to a byte, selects the PCS XAUI-control
word at offset `0x0e0010`, reads it, clears bit zero, and writes it back:

```c
control &= 0xfffffffeU;
```

Selectors two and three use raw address `0x0e0010 + (xmac << 23)`. Other
selectors use `xmac0_pcs_base + 0x0e0010 + sign_extend32(xmac << 24)`.

## Caller Context

The recovered 1G mode sequence and HSGMII configuration call this specialization.
Neither caller uses its incidental pointer return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 28-instruction ARM64 body at `0x18c68` through `0x18cd8`.
- Exact `0xfffffffe` mask and final store.
- Both direct and `xmac0_pcs_base`-relative pointer paths.
- Exhaustive direct xref query found two caller sites.
- IDA type at `0x18c68` updated to the recovered void byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS XAUI-control interpretation of bit zero.
