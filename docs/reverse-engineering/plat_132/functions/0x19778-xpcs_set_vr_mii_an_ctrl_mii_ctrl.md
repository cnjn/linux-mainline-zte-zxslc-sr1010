# 0x19778 xpcs_set_vr_mii_an_ctrl_mii_ctrl

## Status

- Status: complete
- Confidence: verified selector/address branches, byte truncation, bit-eight
  RMW, void return, and its sole direct caller.
- Size: `0x80` bytes, 31 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_vr_mii_an_ctrl_mii_ctrl(u8 xmac, u8 enable)`.

## Semantics

The function truncates both inputs to bytes, selects PCS offset `0x7e0004`,
then replaces bit eight with bit zero of `enable`:

```c
control = (control & 0xfffffeffU) | ((enable & 1U) << 8);
```

Selectors two and three use the raw `0x7e0004 + (xmac << 23)` window. Other
selectors use `xmac0_pcs_base + 0x7e0004 + sign_extend32(xmac << 24)`.

## Caller Context

The sole direct caller is `xpcs_init @ 0x1a420`. It writes enable value one
during PCS initialization and ignores the incidental pointer return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete ARM64 body at `0x19778` through `0x197f4`.
- Exact `UBFIZ W1,W1,#8,#1` insertion and `0xfffffeff` mask.
- Both direct and `xmac0_pcs_base`-relative selector paths.
- Exhaustive direct xref query found only `xpcs_init @ 0x1a420`.
- IDA type at `0x19778` updated to the recovered void two-byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS hardware semantics of bit eight in the VR-MII AN control word.
