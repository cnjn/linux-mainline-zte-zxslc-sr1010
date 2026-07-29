# 0x0208c serdes_set_loopback_mode

## Status

- Status: complete
- Confidence: verified all 492 instructions, 46 basic blocks, snapshot/restore
  lifecycle, mode dispatch, MMIO write order, PRBS option, logs, and returns.
- Size: `0x7e8` bytes, 492 ARM64 instructions.
- Recovered signature: `int serdes_set_loopback_mode(u32 mode,
  int prbs_enable)`.

## State Lifecycle

The function owns a static call counter and six saved 32-bit register values
for SerDes offsets `0x1c`, `0x24`, `0x40`, `0x48`, `0x90`, and `0x94`.

- When the counter is zero, it snapshots all six registers and logs the
  binary's misspelled first-call message.
- When the counter is nonzero, it restores all six defaults before validating
  or applying the requested mode, then logs the recovery message.
- Modes 0-10 log a mode result and increment/return the counter.
- A mode above 10 is rejected after the snapshot/restore step and returns the
  `printk` result without incrementing the counter.

Consequently, mode 10 counts as a completed call even though it takes the
switch default and logs `the path mode is error`. Mode 9 performs no writes
after the entry snapshot/restore and logs `recovery the default data`.

## Mode Transactions

All listed changes are ordered read-modify-writes; omitted fields retain the
entry snapshot/restored value.

| Mode | Binary path label | Key distinctions |
| --- | --- | --- |
| 0 | `PATH1_TX2RX_PCS_LOOP0` | `0x1c[8]=0`, `0x40[16:15]=1`, `0x48[21,18,17,14:12]=0,1,0,0`, `0x94[2:0]=1` |
| 1 | `PATH1_TX2RX_PCS_LOOP1` | mode 0 transaction with `0x94[2:0]=2` |
| 2 | `PATH1_TX2RX_PCS_LOOP2` | mode 0 transaction with `0x94[2:0]=3` |
| 3 | `PATH2_TX2RX_CABLE_LOOP` | clears `0x40[16:15]`, sets `0x94[2:0]=4` |
| 4 | `PATH3_TX2RX_PMA_LOOP` | selects `0x24[20:19]=1`, sets `0x48[17]`, and `0x94[2:0]=5` |
| 5 | `PATH4_RX2TX_PCS_LOOP` | sets `0x1c[8]`, clears selected `0x24`, `0x40`, and `0x48` fields, sets `0x94[2:0]=6` |
| 6 | `PATH5_RX2TX_PMA_LOOP` | sets `0x1c[8]`, `0x24[21]`, and `0x48[21]`; clears `0x94[2:0]` |
| 7 | `PATH6_RX_RECEIVE` | selects `0x48[14:12]=2` and `0x94[2:0]=1` |
| 8 | `PATH6_TRANSMIT` | register transaction identical to mode 7; log differs |
| 9 | recovery | no post-restore register writes |
| 10 | error | no post-restore register writes; still increments counter |

Modes 0-4 and 7-8 also select value 2 in offset `0x24` bits 16-18 and clear
its bits 19-22. Every configuring mode writes offset `0x90` bits 13-14 to
value 2. When `prbs_enable == 1`, modes 0-8 additionally set offset `0x94`
bits 13-15 and bit 31 using two separate RMW writes. No corresponding clear is
performed when the argument differs from 1; entry restore determines their
starting state.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be an
externally consumed SerDes diagnostic API.

## Concurrency

The snapshot values and call counter are unsynchronized globals. Concurrent
calls can race during first-call capture, restore, mode programming, and
counter increment. The six-register restore and each mode transaction are not
atomic.

## Evidence

- Complete disassembly at `0x208c` through `0x2870` (492 instructions).
- Complete 46-block CFG and address-annotated Hex-Rays output.
- Exact static globals at `0x27850` through `0x27868`.
- Exhaustive xref query found no direct callers.
- IDA type at `0x208c` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction, preserving each hardware-visible RMW, is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
