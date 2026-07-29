# 0x19010 xpcs_set_sr_mii_ctrl_duplex_mode

## Status

- Status: complete
- Confidence: verified selector/address branches, byte truncation, low-bit
  input use, bit-eight RMW, void return, and all direct callers.
- Size: `0x80` bytes, 32 ARM64 instructions.
- Recovered signature:
  `void xpcs_set_sr_mii_ctrl_duplex_mode(u8 xmac, u32 state)`.

## Semantics

The function truncates both arguments to bytes, selects the same PCS SR-MII
control word as `xpcs_set_sr_mii_ctrl_speed`, reads it, clears bit eight, and
inserts bit zero of `state` at bit eight. Its write is therefore:

```c
control = (control & 0xfffffeffU) | ((state & 1U) << 8);
```

Selectors two and three use the raw `0x7c0000 + (xmac << 23)` window; other
selectors use the `xmac0_pcs_base`-relative address formula verified for the
speed writer.

## Caller Context

Six direct callers use this helper: the recovered SGMII half-duplex path and
wrapper, USXGMII auto-disable configuration, 1G mode configuration, USXGMII
incidental pointer return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The volatile RMW is
not internally serialized.

## Evidence

- Complete 32-instruction ARM64 body at `0x19010` through `0x1908c`.
- Exact byte truncations at the two entry instructions and bit-eight `UBFIZ`.
- Both direct and base-relative pointer paths.
- `0xfffffeff` mask and final volatile store.
- Exhaustive direct xref query found six caller sites.
- IDA type at `0x19010` updated to the recovered void two-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact hardware meaning of SR-MII control bit eight.
- Original declaration width and vendor name of `state`.
