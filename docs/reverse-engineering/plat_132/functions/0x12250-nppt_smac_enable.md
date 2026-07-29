# 0x12250 nppt_smac_enable

## Status

- Status: complete
- Confidence: verified MAC bound, normal/RGMII/XMAC branches, config-bit set,
  asymmetric SOPC update rules, void return, and sole caller.
- Size: `0x108` bytes, 66 ARM64 instructions.
- Recovered signature: `void nppt_smac_enable(u8 mac)`.

## Semantics

The function returns for `mac > 6`. Otherwise it reads the initial
`g_smac_max_index` and takes one of three paths:

- `mac <= g_smac_max_index`: set bits zero and one in the normal NPPT SMAC
  configuration word at `nppt_base + 0x40000 * (mac + 1)`.
- `mac == 6` outside that range: set the same bits in `rgmii_base + 0`.
- Other out-of-range values: call `xmac_tx_rx_enable((u8)(mac - 4))`.

Post-enable SOPC handling is intentionally asymmetric with disable:

- If `mac == g_smac_max_index` on the second global read, it updates bit `mac`
  at `nppt_base + 0x343f0`: config bit 13 set clears the SOPC bit; clear sets it.
- Otherwise, only `mac == 6` updates fixed SOPC bit six (`0x40`) with the same
  bit-13 polarity.
- Other normal SMAC selectors do not receive a post-enable SOPC update here.

## Caller Context

The sole direct caller is `check_phy @ 0x126e4` at `0x12864`, after link-up
configuration succeeds.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It performs volatile
configuration/SOPC RMW operations or delegates XMAC TX/RX enable; caller
serialization controls the shared state.

## Evidence

- Complete 66-instruction ARM64 body at `0x12250` through `0x12354`.
- Exact byte upper bound six, initial `g_smac_max_index` routing, config
  `ORR #3` writes, and delegated XMAC-enable argument.
- Exact equality-only normal SOPC update and separate MAC6 bit-six branch.
- One direct caller xref from `check_phy`.
- IDA function type updated at `0x12250` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Why only the maximum normal SMAC index receives the post-enable SOPC update.
- Exact meaning of configuration bits zero/one and SOPC bit polarity.
