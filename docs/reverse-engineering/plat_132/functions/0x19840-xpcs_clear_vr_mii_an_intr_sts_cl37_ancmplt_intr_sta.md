# 0x19840 xpcs_clear_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta

## Status

- Status: complete
- Confidence: verified selector/address branches, bit-zero RMW, void return,
  and both direct callers.
- Size: `0x74` bytes, 28 ARM64 instructions.
- Recovered signature:
  `void xpcs_clear_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta(u8 xmac)`.

## Semantics

The function truncates the selector to a byte, selects the PCS AN interrupt
status word at offset `0x7e0008`, reads it, clears bit zero, and writes the word
back:

```c
interrupt_status &= 0xfffffffeU;
```

Selectors two and three use raw address `0x7e0008 + (xmac << 23)`. Other
selectors use `xmac0_pcs_base + 0x7e0008 + sign_extend32(xmac << 24)`.

## Caller Context

The USXGMII and SGMII auto-enable speed/duplex handlers call this function once
they have observed their bit-zero completion indication. Neither caller uses
its incidental pointer return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 28-instruction ARM64 body at `0x19840` through `0x198b0`.
- Exact `0xfffffffe` mask and final volatile store.
- Both direct and `xmac0_pcs_base`-relative pointer paths.
- Exhaustive direct xref query found two caller sites.
- IDA type at `0x19840` updated to the recovered void byte signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact AN event represented by the vendor completion-status bit.
