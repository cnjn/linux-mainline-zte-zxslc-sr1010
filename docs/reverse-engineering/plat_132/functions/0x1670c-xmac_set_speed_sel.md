# 0x1670c xmac_set_speed_sel

## Status

- Status: complete
- Confidence: verified byte selector handling, both register-address formulas,
  32-bit read-modify-write mask/shift, void return, and all nine direct callers.
- Size: `0x70` bytes, 28 ARM64 instructions.
- Recovered signature: `void xmac_set_speed_sel(u8 xmac, u32 speed)`.

## Semantics

The function selects one volatile 32-bit register based on the byte-truncated
XMAC selector:

- For XMAC `2` or `3`, it accesses the raw address
  `(xmac + 7) << 16`, yielding `0x90000` or `0xa0000`.
- For every other byte selector, it accesses
  `nppt_base + (xmac << 18) + 0x140000`.

It reads the selected word, clears bits `31:29` with `0x1fffffff`, and writes
the low three bits of `speed` back into bits `31:29` via `speed << 29`. There
is no selector range check, locking, or error return.

## Caller Context

Nine direct call sites use this as the common XMAC speed-select write primitive:

- `xamc_init_conf_by_speed @ 0x16d6c`.
- `xmac_mode_set @ 0x17bd8`.
- `xmac_speed_process_in_sgmii_auto_mode @ 0x18058`.
- `xmac_config_speed_duplex @ 0x18130`.
- `xmac_speed_process_in_usxgmii_auto_mode @ 0x18530`.
- Both SGMII and USXGMII paths in `xmac_speed_process @ 0x1860c`.
- `phy_051_set_xmac_work_mode @ 0x1bfa4`.
- `phy_051_set_xmac_speed @ 0x1c00c`.

All discard the return register, supporting the recovered void signature.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. This is a volatile
MMIO-style read-modify-write; callers must provide any required serialization.

## Evidence

- Complete 28-instruction ARM64 body at `0x1670c` through `0x16778`.
- Exact byte-truncated `(xmac - 2) <= 1` branch, two address calculations, and
  duplicated address calculation for the read and write.
- Exact `AND #0x1fffffff` plus `ORR speed, LSL #29` sequence.
- Nine direct caller xrefs, including the post-call instruction in
  `xamc_init_conf_by_speed` which reuses its saved XMAC byte rather than `W0`.
- IDA function type updated at `0x1670c` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware identities of the special XMAC-2/3 raw address windows.
- Whether callers serialize this read-modify-write against other register users.
