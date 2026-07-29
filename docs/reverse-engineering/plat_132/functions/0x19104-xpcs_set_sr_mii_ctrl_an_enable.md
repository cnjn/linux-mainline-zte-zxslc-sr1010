# 0x19104 xpcs_set_sr_mii_ctrl_an_enable

## Status

- Status: complete
- Confidence: verified selector/address branches, byte truncation, low-bit
  input use, bit-12 RMW, void return, and all direct callers.
- Size: `0x80` bytes, 32 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_sr_mii_ctrl_an_enable(u8 xmac, u32 enable)`.

## Semantics

The function truncates `xmac` and `enable` to bytes, selects the shared PCS
SR-MII control word, reads it, clears bit 12, and inserts enable bit zero at
bit 12:

```c
control = (control & 0xffffefffU) | ((enable & 1U) << 12);
```

Selectors two and three use the raw `0x7c0000 + (xmac << 23)` window; all other
selectors use the verified `xmac0_pcs_base`-relative address formula.

## Caller Context

Nine direct callers use the writer: the recovered SGMII half-duplex path,
USXGMII and SGMII auto-negotiation configuration, 1000BASE-X auto-negotiation,
SGMII exit, and HSGMII configuration. No caller uses its incidental pointer
return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 32-instruction ARM64 body at `0x19104` through `0x19180`.
- Exact entry byte truncations and `UBFIZ W1,W1,#12,#1`.
- `0xffffefff` mask, both pointer paths, and final volatile store.
- Exhaustive direct xref query found nine caller sites.
- IDA type at `0x19104` updated to the recovered void two-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact PCS role of bit 12 beyond the vendor symbol's AN-enable label.
