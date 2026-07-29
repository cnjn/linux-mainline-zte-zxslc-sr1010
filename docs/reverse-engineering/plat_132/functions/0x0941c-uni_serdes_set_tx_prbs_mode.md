# 0x0941c uni_serdes_set_tx_prbs_mode

## Status

- Status: complete
- Confidence: verified unconditional generator enable, CPU-specific setup,
  PRBS selector effects, return behavior, absent internal xrefs, and exported
  ABI.
- Size: `0x144` bytes, 78 ARM64 instructions.
- Recovered signature: `int uni_serdes_set_tx_prbs_mode(int prbs_mode)`.

## Semantics

Always invokes `uni_serdes_set_gen_en(1)` first, discarding its log return.

- CPU 133 sets `+0x24` bit 17, sets low `+0x94` bits to 4, then sets bit 31.
- Otherwise, CPU 129 clears `+0x24` bits 16-18; programs `+0x94` bits 8-9 to
  binary 2, bits 10-11 to binary 2, low bits to 4, and clears bit 31.
- Other CPU types skip this setup.

It then selects the TX PRBS field at `+0x94` bits 16-18:

| `prbs_mode` | Field value | Vendor label |
| --- | --- | --- |
| 0 | `0` | PRBS7 |
| 1 | `4` | PRBS23 |
| 2 | `5` | PRBS31 |
| other | unchanged | none |

The function returns zero on every path, including unsupported selectors.

## Caller Context

No internal IDB xrefs target this entry. It is exported through
`__ksymtab_uni_serdes_set_tx_prbs_mode`.

## Evidence

- Complete ARM64 body at `0x941c` through `0x955c`.
- Unconditional generator enable call at `0x9430`.
- CPU-133 gate/setup at `0x9434`-`0x9470`; CPU-129 fallback at
  `0x9474`-`0x94cc`.
- PRBS23, PRBS7, and PRBS31 selector paths at `0x952c`-`0x9540`,
  `0x950c`-`0x951c`, and `0x94e4`-`0x94fc`.
- Constant-zero return at `0x9550`.
- IDA type at `0x941c` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
