# 0x1964c xpcs_set_vr_mii_an_ctrl_sgmii_link_sts

## Status

- Status: complete
- Confidence: verified selector/address branches, byte truncation, low-bit
  input use, bit-four RMW, void return, and all direct callers.
- Size: `0x80` bytes, 31 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_vr_mii_an_ctrl_sgmii_link_sts(u8 xmac, u32 link_status)`.

## Semantics

The function truncates `xmac` and `link_status` to bytes, selects a companion
PCS word at offset `0x7e0004`, reads it, clears bit four, and inserts link-status
bit zero at bit four:

```c
link_control = (link_control & 0xffffffefU) |
               ((link_status & 1U) << 4);
```

Selectors two and three use the raw `0x7e0004 + (xmac << 23)` window. Other
selectors use `xmac0_pcs_base + 0x7e0004 + sign_extend32(xmac << 24)`.

## Caller Context

Five direct callers use the writer: the recovered SGMII half-duplex helper,
the explicit speed/duplex wrapper, SGMII auto-negotiation configuration, SGMII
exit, and HSGMII configuration. No caller uses its incidental pointer return
register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 31-instruction ARM64 body at `0x1964c` through `0x196c8`.
- Exact byte truncations, `UBFIZ W1,W1,#4,#1`, and `0xffffffef` mask.
- Both direct and `xmac0_pcs_base`-relative pointer paths.
- Exhaustive direct xref query found five caller sites.
- IDA type at `0x1964c` updated to the recovered void two-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS hardware semantics of the companion word and bit four.
