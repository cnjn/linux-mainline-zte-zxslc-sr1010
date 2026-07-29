# 0x19090 xpcs_speed_duplex_conf_in_auto_disable_usxgmii_mode

## Status

- Status: complete
- Confidence: verified three-argument ABI, selector gate, speed/duplex order,
  delay/reset/wait sequence, status return, and no direct code xrefs.
- Size: `0x74` bytes, 28 ARM64 instructions.
- Recovered signature:
  `int xpcs_speed_duplex_conf_in_auto_disable_usxgmii_mode(u8 xmac, u32 speed, u32 duplex)`.

## Semantics

The helper truncates `xmac` to a byte. A selector above four logs
`xmac_index(%d) is error` and returns `-1`. For valid selectors it:

1. Writes SR-MII speed from `speed`.
2. Writes SR-MII duplex control from `duplex`.
3. Calls `__const_udelay(859000)`.
4. Enables the VR-XS/PCS USRA reset control with literal one.
5. Returns `xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared(xmac)` unchanged.

The entry `MOV W6,W2` preserves the original third argument across the speed
writer; it is the duplex input passed to the second PCS call.

## Caller Context

No direct code xrefs exist in the current IDB. The function is retained as a
complete callable vendor helper; it may be reached indirectly or be unused in
this module build.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. The lower PCS calls
and reset wait determine synchronization behavior.

## Evidence

- Complete 28-instruction ARM64 body at `0x19090` through `0x19100`.
- Saved third argument at `0x190c0`, later passed as the duplex writer input.
- Selector check, exact `0x0d1b78` delay, literal-one USRA write, and tail wait
  return.
- Exhaustive direct xref query returned zero caller sites.
- IDA type at `0x19090` updated to the recovered three-argument `int` signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Whether this helper is called indirectly or is unused support code in this
  module build.
