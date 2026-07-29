# 0x12178 nppt_smac_disable

## Status

- Status: complete
- Confidence: verified MAC bound, normal/RGMII/XMAC branches, config-bit clear,
  SOPC update predicate, void return, and sole caller.
- Size: `0xd8` bytes, 54 ARM64 instructions.
- Recovered signature: `void nppt_smac_disable(u8 mac)`.

## Semantics

The function returns immediately for `mac > 6`. Otherwise it reads the current
`g_smac_max_index` and takes one of three branches:

- `mac <= g_smac_max_index`: clear bits zero and one in the normal NPPT SMAC
  configuration word at `nppt_base + 0x40000 * (mac + 1)`.
- `mac == 6` outside that range: clear the same bits in `rgmii_base + 0`.
- Other out-of-range values: call `xmac_tx_rx_disable((u8)(mac - 4))`.

It then rereads `g_smac_max_index`. Only if `mac` is now within the normal SMAC
range does it update bit `mac` at `nppt_base + 0x343f0`: config bit 13 set
clears the SOPC bit; config bit 13 clear sets it. The MAC6 and XMAC branches do
not perform this final SOPC update under the normal runtime maximum.

## Caller Context

The sole direct caller is `check_phy @ 0x126e4` at `0x12770`, when the PHY
callback reports link down.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It performs volatile
configuration/SOPC RMW operations or delegates XMAC TX/RX disable; caller
serialization controls the shared state.

## Evidence

- Complete 54-instruction ARM64 body at `0x12178` through `0x1224c`.
- Exact byte MAC upper bound six, initial and second `g_smac_max_index` reads,
  normal/RGMII/XMAC routing, and config `AND #0xfffffffc` writes.
- Exact post-branch bit-13 test and SOPC bit update.
- One direct caller xref from `check_phy`.
- IDA function type updated at `0x12178` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact meaning of configuration bits zero/one and their relation to link down.
- Why MAC6 skips the final normal-SOPC update under the typical maximum index.
