# 0x1c00c phy_051_set_xmac_speed

## Status

- Status: complete
- Confidence: verified unsigned speed gate, both PCS-mode branches, helper
  argument flow, no-op paths, logging, caller, and void return.
- Size: `0xb4` bytes, 45 ARM64 instructions.
- Recovered signature:
  `void phy_051_set_xmac_speed(u8 xmac, u32 uni_speed, u32 pcs_mode)`.

## Semantics

The function rejects `uni_speed > 6` without any side effect. For an accepted
speed, it handles only two raw PCS modes:

- Mode `3` initializes a local XMAC speed to zero, calls
  `xmac_switch_uni_speed_to_xmac_speed(xmac, uni_speed, &xmac_speed)`, then
  calls `xmac_set_speed_sel(xmac, xmac_speed)` and logs the change. The
  conversion helper stores values for input speeds one through six; therefore
  accepted input zero retains the initialized output value zero.
- Mode `6` calls `xmac_speed_process_in_sgmii_auto_mode(xmac)`. The callee
  reads only its XMAC argument, so this path deliberately does not consume the
  supplied speed.

All other accepted PCS modes are no-ops. The function has no meaningful return
value.

`uni_speed` is modeled as `u32`: the sole caller supplies a zero-extended byte,
but this function compares and forwards its full `W1` register value, so the
original source declaration width is not independently verified.

## Caller Context

The sole direct in-module caller is `phy_zxic051_check @ 0x1c0c0` at `0x1c2f4`.
It invokes this helper after PHY/XMAC link reconciliation reports an outer-speed
mismatch.

## Concurrency and Ownership

No local lock, allocation, ownership transfer, global write, or direct MMIO
access occurs. Hardware changes are delegated to the selected speed helper.

## Evidence

- Complete 45-instruction ARM64 body at `0x1c00c` through `0x1c0bc`.
- Unsigned `CMP W1, #6` gate; exact comparisons against PCS modes `3` and `6`.
- Direct calls to `xmac_switch_uni_speed_to_xmac_speed @ 0x17fdc`,
  `xmac_set_speed_sel @ 0x1670c`, and
  `xmac_speed_process_in_sgmii_auto_mode @ 0x18058`.
- Complete callee body confirms the mode-six helper consumes only `W0`.
- One direct code xref from `phy_zxic051_check`.
- IDA function type updated at `0x1c00c` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of raw PCS modes `3` and `6` at this PHY-family boundary.
- Why the mode-three path permits zero while the conversion helper has no
  explicit zero-input case.
