# 0x1874c xmac_set_pcs_for_sgmii_half_duplex

## Status

- Status: complete
- Confidence: verified mode/auto gates, full argument ABI, PCS-call order, AN
  state branches, void return, and both direct caller sites.
- Size: `0xa4` bytes, 40 ARM64 instructions.
- Recovered signature:
  `void xmac_set_pcs_for_sgmii_half_duplex(u32 xmac, u32 configure, u32 speed, u32 state)`.

## Semantics

The helper first reads `sg_xmac_work_mode[xmac]` using the full unsigned
selector and requires the cached value to equal three. It then reads
`g_xmac_work_in_auto[xmac]` using that same selector and returns when the byte
is zero. Only after both gates does it truncate the selector to a byte for PCS
operations.

When `configure == 1`, it calls
`xpcs_set_speed_duplex_in_sgmii_anto_disale_mode(pcs_xmac, speed, state)`, then
clears SR-MII AN enable. The callee's assembly proves that the third argument is
passed in `W2`: it sets PCS speed from `speed`, PCS duplex from `state`, and
writes SGMII link status one.

For every other `configure` value, it first reads SR-MII AN enable. A nonzero
bit returns without a PCS write. When AN is clear, it writes the cached mode
value three as PCS speed, writes state one as PCS duplex, writes SGMII link
status zero, and enables AN.

## Caller Context

`check_phy @ 0x126e4` is the only direct caller:

- At `0x12790`, its XMAC link-down path calls `(xmac_slot, 0, 3, 1)` after
  disabling the corresponding SMAC.
- At `0x12848`, its XMAC link-up path calls `(xmac_slot, 1, phy_speed, 1)` only
  after `xmac_config_speed_duplex` and only when decoded PHY duplex equals zero.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer is present. The
function is reached from the PHY polling thread through `check_phy` and races
with any independent writers of the two shared XMAC-state arrays or PCS words.

## Evidence

- Complete 40-instruction ARM64 body at `0x1874c` through `0x187ec`.
- The raw-index load of `sg_xmac_work_mode`, exact equality test against three,
  byte auto-flag gate, and post-gate `UXTB` selector truncation.
- `MOV W2,W3` at `0x18788`, followed by the call at `0x1879c`, proves the
  third `state` argument reaches the PCS duplex helper.
- Direct disassembly of `xpcs_set_speed_duplex_in_sgmii_anto_disale_mode`,
  `xpcs_sr_mii_ctrl_is_an_enable`, and `xpcs_set_sr_mii_ctrl_an_enable`.
- Both direct xrefs from `check_phy`.
- IDA type at `0x1874c` updated to the recovered void four-`unsigned int`
  signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact hardware meanings of the PCS `state` and SGMII link-status fields.
- Why the configure-one caller is restricted to a decoded PHY duplex value of
  zero while it passes literal state one to the PCS helper.
