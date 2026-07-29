# 0x1c0c0 phy_zxic051_check

## Status

- Status: complete
- Confidence: verified two-slot selection, callback output order, mode-history
  filter, both threshold-recovery paths, return encoding, callback registration,
  and all XMAC mode-set calls. Field semantics remain strong inferences where
  established only by usage.
- Size: `0x2f4` bytes, 189 ARM64 instructions.
- Recovered signature: `int phy_zxic051_check(u8 phy_id)`.

## Semantics

The function derives `xmac = (byte_266BC != phy_id)`, yielding slot zero or one,
then queries physical PHY `xmac + 4`. The callback writes an unused raw word,
a PHY-link byte, an outer speed byte, and a duplex byte. The outer speed byte is
converted in place by `phy_zxic_speed_outer2uni`.

It reads `pcs_mode = sg_phy_speed_mode_cur_mac[xmac]`. On a mode change from
`sg_last_serdes_mode0_54532[xmac]`, it logs and, only for CPU 133/129, calls
`xmac_mode_set(xmac, pcs_mode, speed, duplex)` when all conditions hold:

```c
pcs_mode != 12 &&
(last_mode0 != 12 || pcs_mode != last_mode1) &&
(isCpuType_133() || isCpuType_129())
```

It then shifts histories unconditionally for the changed-mode path:
`last_mode1 = last_mode0; last_mode0 = pcs_mode`.

If the PHY-link byte is zero, it returns `-1`. With PHY link present, it asks
for XMAC/NPPT global link state:

- If global link is down, it increments `sg_051_interval_cnt_54535[xmac]`.
  Before the configurable global threshold it returns `-1`; at the threshold it
  replays `xmac_mode_set` on CPU 133/129, resets the interval counter, and
  returns `-1`.
- If global link is up, it processes XMAC speed and reads its current UNI speed.
  It clears the interval counter. A speed mismatch calls
  `phy_051_set_xmac_speed(xmac, speed, pcs_mode)`, increments
  `sg_051_re_an_cnt_54536[xmac]`, and follows the same threshold/replay/reset
  sequence before returning `-1`.
- Only when PHY link is present, global link is up, and speed matches does it
  clear a nonzero re-negotiation counter and return
  `speed | ((duplex & 1) << 10)`.

## Caller Context

`xmac_zxic_phy_init @ 0x18348` installs this function through data references
at `0x18374` and `0x18384` as the PHY-family check callback. There are no direct
in-module `BL` call sites.

## Concurrency and Ownership

This runs through the XMAC PHY callback machinery. It mutates shared mode
history and recovery counters without local locking, and may reprogram XMAC
state through `xmac_mode_set`. No allocation or ownership transfer occurs.

## Evidence

- Complete 189-instruction ARM64 body at `0x1c0c0` through `0x1c3b0`.
- Exact callback output-pointer registers and two-slot calculation.
- History sentinel `12`, CPU checks, threshold counter comparisons, reset paths,
  and final speed/duplex encoding.
- Three direct `xmac_mode_set` sites and callback registration references from
  `xmac_zxic_phy_init`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact vendor semantics of the callback's unused raw word, mode sentinel 12,
  and global threshold value.
- Synchronization/lifetime contract for the PHY callback and shared counters.
- Why only two XMAC slots are selected from the incoming PHY byte.
