# 0x09784 uni_check_serdes_lock

## Status

- Status: complete
- Confidence: verified CPU-dependent PLL source, separate CDR/ALOS reads,
  final log return, absent internal xrefs, and exported ABI.
- Size: `0x58` bytes, 22 ARM64 instructions.
- Recovered signature: `int uni_check_serdes_lock(void)`.

## Semantics

Logs three diagnostic bits:

| Value | CPU 129 source | Other CPU source |
| --- | --- | --- |
| PLL status | `uni_serdes_base + 0xcc`, bit 1 | `+0xd0`, bit 0 |

It then independently reads `+0xe4` bit 1 as CDR status and bit 0 as ALOS
data. It returns the final `printk` result. The entry is exported through
`__ksymtab_uni_check_serdes_lock` and has no internal IDB xrefs.

## Evidence

- Complete ARM64 body at `0x9784` through `0x97d8`.
- CPU-129 predicate at `0x978c`-`0x979c`; PLL sources at `0x97a0` and `0x97ac`.
- Separate `+0xe4` loads at `0x97b8` and `0x97bc`; CDR/ALOS extraction at
  `0x97c4` and `0x97cc`.
- Final returned log call at `0x97d0`.
- IDA type at `0x9784` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
