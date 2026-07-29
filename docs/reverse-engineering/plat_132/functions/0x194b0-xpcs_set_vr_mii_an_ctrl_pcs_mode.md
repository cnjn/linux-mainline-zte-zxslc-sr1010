# 0x194b0 xpcs_set_vr_mii_an_ctrl_pcs_mode

## Status

- Status: complete
- Confidence: verified selector/address branches, two-bit input use,
  bits-one/two RMW, void return, and both direct callers.
- Size: `0x7c` bytes, 30 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_vr_mii_an_ctrl_pcs_mode(u8 xmac, u32 pcs_mode)`.

## Semantics

The function truncates `xmac` to a byte, selects the PCS word at offset
`0x7e0004`, reads it, clears bits one and two, then inserts input bits zero and
one at those positions:

```c
control = (control & 0xfffffff9U) | ((pcs_mode & 3U) << 1);
```

Selectors two and three use raw address `0x7e0004 + (xmac << 23)`. Other
selectors use `xmac0_pcs_base + 0x7e0004 + sign_extend32(xmac << 24)`.

## Caller Context

The recovered 1G PCS sequence calls this helper after a successful PSEQ wait.
`xpcs_hsgmii_mode_conf` also calls it during HSGMII configuration. Neither caller
uses its incidental pointer return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 30-instruction ARM64 body at `0x194b0` through `0x19528`.
- `UBFIZ W1,W1,#1,#2` proves raw input bits zero/one map to bits one/two.
- Exact `0xfffffff9` mask and final store.
- Both direct and `xmac0_pcs_base`-relative pointer paths.
- Exhaustive direct xref query found the two caller sites.
- IDA type at `0x194b0` updated to the recovered void `(u8, u32)` signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact physical decoding of the two VR-MII PCS-mode bits.
