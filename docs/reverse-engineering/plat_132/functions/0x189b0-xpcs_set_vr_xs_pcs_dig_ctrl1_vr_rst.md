# 0x189b0 xpcs_set_vr_xs_pcs_dig_ctrl1_vr_rst

## Status

- Status: complete
- Confidence: verified selector/address branches, byte input, bit-15 RMW, void
  return, and sole direct caller.
- Size: `0x80` bytes, 32 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_vr_xs_pcs_dig_ctrl1_vr_rst(u8 xmac, u8 enable)`.

## Semantics

The function truncates both inputs to bytes, selects the PCS word at offset
`0x0e0000`, reads it, clears bit 15, and inserts reset bit zero:

```c
control = (control & 0xffff7fffU) | ((enable & 1U) << 15);
```

Selectors two and three use raw address `0x0e0000 + (xmac << 23)`. Other
selectors use `xmac0_pcs_base + 0x0e0000 + sign_extend32(xmac << 24)`.

## Caller Context

`xpcs_usxgmii_mode_conf @ 0x1a210` is the sole direct caller. It requests reset
with literal one after writing its USXGMII mode field, then calls the reset wait
helper without inspecting its result.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 32-instruction ARM64 body at `0x189b0` through `0x18a2c`.
- Exact byte truncations, `UBFIZ W1,W1,#15,#1`, and `0xffff7fff` mask.
- Both direct and `xmac0_pcs_base`-relative pointer paths.
- Exhaustive direct xref query found only the call at `0x1a2e8`.
- IDA type at `0x189b0` updated to the recovered void byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact reset timing and hardware behavior associated with bit 15.
