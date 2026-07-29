# 0x02de8 serdes_set_rx_prbs_mode

## Status

- Status: complete
- Confidence: verified all 101 instructions, CPU-type precedence and writes,
  five mode transactions, two callers, logs, and constant-zero return.
- Size: `0x1a8` bytes, 101 ARM64 instructions, 17 basic blocks.
- Recovered signature: `int serdes_set_rx_prbs_mode(u32 mode)`.

## CPU-Specific Setup

The function applies the first matching CPU setup before mode validation:

| CPU predicate | Ordered setup |
| --- | --- |
| 132 | clear `0x48[14:12]`; separately set `0x48[16]`; set `0x94[2:0]=4` |
| 133 | set `0x48[14:12]=2`; set `0x94[2:0]=4`; set `0x94[31]` |
| 129 | same as 133, but first set `0x94[9:8]=2` and `0x94[11:10]=2` |
| none | no setup |

Predicates are tested in 132, 133, 129 order and must equal exactly 1. The two
CPU-132 writes to offset `0x48` are preserved as separate RMW operations.

## Mode Transactions

Modes 0-4 replace offset `0x94` bits 19-21:

| Mode | Field value | Logged PRBS |
| --- | --- | --- |
| 0 | 0 | 7 |
| 1 | 4 | 23 |
| 2 | 5 | 31 |
| 3 | 1 | 9 |
| 4 | 3 | 15 |

Other values receive any CPU-specific setup but no mode write or mode log.
Every input returns zero; logging results are discarded.

## Caller Context

- `serdes_set_sprbsrxbist @ 0x2f90` passes its PRBS mode minus one, using
  unsigned 32-bit arithmetic.
- `serdes_get_hard_prbs_cnt @ 0x43ec` forwards its second unsigned argument.

## Evidence

- Complete disassembly at `0x2de8` through `0x2f8c` (101 instructions).
- Complete 17-block CFG and address-annotated decompilation.
- Exact CPU helper call order, masks, constants, and five-way switch.
- Direct call xrefs at `0x2fa8` and `0x4420`.
- IDA type at `0x2de8` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction, preserving ordered RMWs, is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
