# 0x123ec nppt_smac_config_speed_duplex

## Status

- Status: complete
- Confidence: verified byte-truncated inputs, all config-bit cases, normal/MAC6
  windows, initial and post-reset SOPC updates, reset sequence, gate handling,
  send-enable tail, void return, and sole caller.
- Size: `0x2f8` bytes, 186 ARM64 instructions.
- Recovered signature:
  `void nppt_smac_config_speed_duplex(u8 mac, u8 speed, u8 duplex)`.

## Semantics

The function ignores `mac > 6`. For other byte selectors it obtains an old
configuration word from `nppt_base + 0x40000 * (mac + 1)` when
`mac <= g_smac_max_index`, with `mac == 6` overriding the source with
`rgmii_base + 0`.

It derives a new configuration word from raw byte `speed` and boolean `duplex`:

- `speed == 3`: duplex set clears bit 15 and sets bit 13; duplex clear clears
  both bits 15 and 13.
- Other speeds: duplex set sets bits 15/13; duplex clear sets bit 15 and clears
  bit 13. Bit 14 is set only for `speed == 2` and cleared for all other values.

It writes the new word back to the initial normal and/or MAC6 target, then
updates bit `mac` in `nppt_base + 0x343f0`: new bit 13 set clears the SOPC bit;
new bit 13 clear sets it. This first SOPC update happens even when no reset is
needed.

If old and new bit 13 match, the function returns. On a bit-13 change it logs,
conditionally disables the CPU-133 auto-gate only when its saved value equals
one, calls `smac_reset(1 << mac)`, and unconditionally writes the new word to
the NPPT SMAC configuration slot. It then:

- For `mac <= g_smac_max_index`, repeats the SOPC-bit update; writes packet
  filter `0x80000001`, AN control `0x11200`, and ORs bits `0x2`/`0x20` into
  offsets `0xd00`/`0xd30` in the NPPT SMAC block.
- Otherwise, for `mac == 6`, repeats the SOPC-bit update and performs the same
  packet/filter/OR sequence through the RGMII window.
- Calls `sopc_send_enable(mac)` in every reset path, logs it, and conditionally
  restores the CPU-133 auto-gate after a second CPU check.

## Caller Context

The sole direct caller is `check_phy @ 0x126e4` at `0x1285c`, for PHY slots
that are not XMAC slots four/five. It supplies the low byte of the PHY status
and the decoded duplex bit.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The function updates
multiple shared volatile register words and performs reset/send-enable actions;
caller context must provide any required serialization.

## Evidence

- Complete 186-instruction ARM64 body at `0x123ec` through `0x126e0`.
- Exact input byte truncations, MAC upper bound six, g_smac_max_index tests,
  RGMII override, and all four speed/duplex configuration cases.
- Exact initial SOPC RMW, bit-13 change predicate, CPU-133 gate sequence,
  reset/write ordering, normal/MAC6 post-reset branches, and send-enable tail.
- One direct caller xref from `check_phy`.
- IDA function type updated at `0x123ec` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meanings of config bits 13 through 15 and SOPC-bit polarity.
- Why the reset path always writes the NPPT SMAC config slot even for MAC6.
- Required synchronization for the multi-register duplex transition.
