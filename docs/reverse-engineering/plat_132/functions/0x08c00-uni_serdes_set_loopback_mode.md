# 0x08c00 uni_serdes_set_loopback_mode

## Status

- Status: complete
- Confidence: verified persistent default snapshot, restore ordering, bounds and
  CPU gate, all nine hardware profiles, auxiliary-flag behavior, return counter,
  and exported ABI.
- Size: `0x81c` bytes, 505 ARM64 instructions.
- Recovered signature:
  `int uni_serdes_set_loopback_mode(uint32_t loopback_mode, int enable_aux_flags)`.

## Semantics

On its first invocation, saves the seven Uni SerDes words at offsets `0x1c`,
`0x24`, `0x40`, `0x48`, `0x60`, `0x90`, and `0x94`. Every later invocation
restores those words in the same order before any mode validation or profile
programming.

Modes above 10 only log `the loop mode is big than PATH_MODE` and do not advance
the persistent call counter. Valid modes advance the counter and return its new
value. Only CPU type 133 applies loopback profile transactions; other CPUs still
perform the snapshot/restore behavior and advance the counter for valid modes.

| Mode | Vendor profile log |
| --- | --- |
| 0 | `PATH1_TX2RX_PCS_LOOP0` |
| 1 | `PATH1_TX2RX_PCS_LOOP1` |
| 2 | `PATH1_TX2RX_PCS_LOOP2` |
| 3 | `PATH2_TX2RX_CABLE_LOOP` |
| 4 | `PATH3_TX2RX_PMA_LOOP` |
| 5 | `PATH4_RX2TX_PCS_LOOP` |
| 6 | `PATH5_RX2TX_PMA_LOOP` |
| 7 | `PATH6_RX_RECEIVE` |
| 8 | `PATH6_TRANSMIT` |
| 9 | logs default recovery after the preceding restore |
| 10 | logs `the path mode is error` |

For CPU-133 modes 0 through 8, `enable_aux_flags == 1` independently appends
two RMW writes at `+0x94`, setting mask `0xe000` and bit 31. Every profile
otherwise preserves the exact ordered RMW sequence in the source reconstruction.

## Return Semantics

For modes 0 through 10, returns the incremented persistent call counter. For an
out-of-range mode, returns the error `printk` status. This mixed but observable
contract supports an `int` ABI.

## Caller Context

No internal IDB xrefs target this entry. It is exported through
`__ksymtab_uni_serdes_set_loopback_mode` for external control paths.

## Evidence

- Complete ARM64 body at `0x8c00` through `0x9418`.
- First snapshot at `0x8c24`-`0x8c60`; subsequent restore at
  `0x8c70`-`0x8ca4`.
- Bounds check at `0x8cb4`-`0x8cc8`; CPU-133 gate at `0x8ccc`-`0x8cd4`.
- CPU-133 profile cases span `0x8cdc`-`0x93d8`; mode 9/default labels are at
  `0x93e4` and `0x93f0`.
- Persistent counter increment at `0x93fc`-`0x9408`.
- IDA type at `0x8c00` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
