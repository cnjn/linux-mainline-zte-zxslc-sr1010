# 0x1952c xpcs_1g_mode_conf

## Status

- Status: complete
- Confidence: verified four-argument ABI, fixed configuration sequence, PSEQ
  result branch, low-power behavior, PCS-mode write, status return, and both
  direct callers.
- Size: `0xa0` bytes, 40 ARM64 instructions.
- Recovered signature:
  `int xpcs_1g_mode_conf(u8 xmac, u32 speed, u32 duplex, u32 pcs_mode)`.

## Semantics

The helper truncates `xmac` to a byte and preserves its remaining three inputs.
It executes the following sequence without its own selector validation:

1. Write PCS type one through `xpcs_set_sr_xs_pcs_ctrl2_pcs_type`.
2. Call the fixed XAUI mode, PMA speed-select, and XS/PCS speed-select helpers.
3. Write SR-MII speed from `speed` and duplex control from `duplex`.
4. Enable XS/PCS low-power with literal one.
5. Wait for the fixed PSEQ state.
6. If the wait returns nonzero, return `-1` and leave low-power enabled.
7. Otherwise disable low-power, write VR-MII AN PCS mode from `pcs_mode`, and
   return zero.

## Caller Context

Two wrappers call this helper:

- `xpcs_1000base_x_conf @ 0x19f74` passes its speed and duplex inputs with
  literal PCS mode zero.
- `xpcs_sgmii_mode_conf @ 0x1a19c` passes its mode/configuration inputs with
  literal PCS mode two.

Both wrappers call `xpcs_prepare_for_switch_mode` first and update
`sg_xpcs_mode[xmac]` after this helper returns, even if it returned `-1`.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The lower PCS calls
and PSEQ wait determine the hardware synchronization behavior.

## Evidence

- Complete 40-instruction ARM64 body at `0x1952c` through `0x195c8`.
- Saved `W1`, `W2`, and `W3` inputs and exact use as speed, duplex, and PCS
  mode at the later call sites.
- Direct analysis of `xpcs_set_sr_xs_pcs_ctrl2_pcs_type` verifies it consumes
  only `(xmac, 1)` despite residual argument registers at the call.
- Complete assembly for both wrapper callers proves literal PCS modes zero and
  two and argument forwarding order.
- IDA type at `0x1952c` updated to the recovered four-argument `int` signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact hardware meanings of the fixed constprop configuration helpers.
- Exact PSEQ state condition and whether callers rely on its low-power-on
  failure state for a later retry.
