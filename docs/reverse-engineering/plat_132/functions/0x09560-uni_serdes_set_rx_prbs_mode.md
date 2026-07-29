# 0x09560 uni_serdes_set_rx_prbs_mode

## Status

- Status: complete
- Confidence: verified CPU-specific RX setup, PRBS selector effects, caller
  edges, constant-zero return, and exported ABI.
- Size: `0x148` bytes, 79 ARM64 instructions.
- Recovered signature: `int uni_serdes_set_rx_prbs_mode(int prbs_mode)`.

## Semantics

- CPU 133 programs `+0x48` bits 12-16, sets low `+0x94` bits to 4, then sets
  bit 31.
- Otherwise, CPU 129 clears `+0x48` bits 12-14; programs `+0x94` bits 8-9 to
  binary 2, bits 10-11 to binary 2, low bits to 4, and clears bit 31.
- Other CPU types skip setup.

It selects RX PRBS field `+0x94` bits 19-21:

| `prbs_mode` | Field value | Vendor log |
| --- | --- | --- |
| 0 | `0` | PRBS7 |
| 1 | `4` | PRBS23 |
| 2 | `5` | PRBS31 |
| other | unchanged | none |

The vendor labels say `tx` even in this RX routine; that string is retained.
Every path returns zero.

## Caller Context

Called by `uni_serdes_set_sprbsrxbist @ 0x96a8` at `0x9704` and
`uni_serdes_get_hard_prbs_cnt @ 0x97dc` at `0x9810`. It is exported through
`__ksymtab_uni_serdes_set_rx_prbs_mode`.

## Evidence

- Complete ARM64 body at `0x9560` through `0x96a4`.
- CPU-133 gate/setup at `0x9570`-`0x95b8`; CPU-129 fallback at
  `0x95bc`-`0x9614`.
- PRBS23, PRBS7, and PRBS31 selector paths at `0x9674`-`0x9688`,
  `0x9654`-`0x9664`, and `0x962c`-`0x9644`.
- Constant-zero return at `0x9698`.
- IDA type at `0x9560` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
