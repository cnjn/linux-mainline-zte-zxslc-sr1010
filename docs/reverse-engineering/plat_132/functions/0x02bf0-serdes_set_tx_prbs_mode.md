# 0x02bf0 serdes_set_tx_prbs_mode

## Status

- Status: complete
- Confidence: verified all 116 instructions, CPU-type precedence and writes,
  six mode transactions, logs, no-xref context, and constant-zero return.
- Size: `0x1f8` bytes, 116 ARM64 instructions, 17 basic blocks.
- Recovered signature: `int serdes_set_tx_prbs_mode(u32 mode)`.

## Common Setup

The function always calls `serdes_set_gen_en(1)` before validating `mode` and
then applies the first matching CPU-specific setup:

| CPU predicate | Ordered setup |
| --- | --- |
| 132 | clear offset `0x24` bits 16-18; set offset `0x94` bits 0-2 to 4 |
| 133 | set `0x24[18:16]=2`; set `0x94[2:0]=4`; set `0x94[31]` |
| 129 | same as 133, but first set `0x94[9:8]=2` and `0x94[11:10]=2` |
| none | no additional setup |

Predicates are tested in 132, 133, 129 order and must equal exactly 1.

## Mode Transactions

| Mode | PRBS selection / action | Additional writes |
| --- | --- | --- |
| 0 | PRBS7: `0x94[18:16]=0` | none |
| 1 | PRBS23: `0x94[18:16]=4` | `0xa4=0x00555555` |
| 2 | PRBS31: `0x94[18:16]=5` | `0xa4=0x00555555` |
| 3 | PRBS9: `0x94[18:16]=1` | none |
| 4 | PRBS15: `0x94[18:16]=3` | none |
| 5 | fixed `0101` pattern | disable generator; write `0x9c=0x55555500`, `0xa0=0x55555555`, `0xa4=0x07555555` |
| >5 | no mode-specific operation | generator and CPU setup still occur |

Modes 0-5 print their exact binary labels. Every input returns zero; `printk`
results and both generator-helper results are discarded.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be an
externally consumed PRBS diagnostic API.

## Evidence

- Complete disassembly at `0x2bf0` through `0x2de4` (116 instructions).
- Complete 17-block CFG and address-annotated decompilation.
- Exact CPU helper call order, register masks, constants, and six-way switch.
- Exhaustive xref query found no direct callers.
- IDA type at `0x2bf0` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction, preserving ordered RMWs, is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
