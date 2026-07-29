# 0x18b3c xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared

## Status

- Status: complete
- Confidence: verified selector/address branches, reset-bit poll, retry/delay
  behavior, status return, and all direct callers.
- Size: `0x90` bytes, 36 ARM64 instructions.
- Recovered signature:
  `int xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared(u8 xmac)`.

## Semantics

The helper truncates its selector to a byte and polls bit 15 of the PCS word at
offset `0x0e0000`. Selectors two and three use the raw window; other selectors
use the mapped `xmac0_pcs_base` path. It reloads that selector-specific word on

It performs at most 400 iterations:

1. Read the word.
2. Return zero immediately when bit 15 is clear.
3. Otherwise call `__const_udelay(859000)`.
4. After the 400th delay, return `-1`.

## Caller Context

Three callers use the wait:

- `xpcs_speed_duplex_conf_in_auto_disable_usxgmii_mode` returns its status
  directly.
- `xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode` returns its status after
  committing decoded speed and duplex outputs.
- `xpcs_usxgmii_mode_conf` ignores the result and continues to cached-mode
  classification.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. Register values and
the mapped PCS base can be observed anew on each poll iteration.

## Evidence

- Complete 36-instruction ARM64 body at `0x18b3c` through `0x18bc8`.
- Counter initialization to `0x190`, bit-15 `TBZ`, and exact delay constant
  `0x0d1b78`.
- Both raw and `xmac0_pcs_base`-relative address paths inside the loop.
- Complete assembly for all three callers confirms two propagated and one
  discarded status path.
- IDA type at `0x18b3c` updated to the recovered byte-selector `int` signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact time unit represented by `__const_udelay(859000)`.
- Whether bit 15 self-clearing is guaranteed on every PCS window variant.
