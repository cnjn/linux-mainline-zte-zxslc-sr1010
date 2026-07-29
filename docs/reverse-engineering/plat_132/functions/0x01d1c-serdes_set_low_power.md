# 0x01d1c serdes_set_low_power

## Status

- Status: complete
- Confidence: verified mode dispatch, SerDes offset, low-byte values, distinct
  error paths, log strings, return behavior, and absence of direct xrefs.
- Size: `0x10c` bytes, 60 ARM64 instructions.
- Recovered signature: `int serdes_set_low_power(u32 mode)`.

## Semantics

Programs the low byte at SerDes offset `0x5c` according to `mode` while
preserving bits 8-31:

| Mode | Low-byte operation | Log |
| --- | --- | --- |
| 0 | clear to `0x00` | `enter normal mode` |
| 1 | OR with `0xff` | `enter low power mode` |
| 2 | replace with `0xdd` | `enter sleep mode` |
| 3 | replace with `0x22` | `enter small flow mode` |
| 4 | replace with `0x33` | `enter rx en and tx off mode` |
| 5 | no register write | `the low power mode is error` |
| >5 | no register write | `LOW POWER MODE IS ERROR` |

Every path returns the corresponding `printk` result. Mode 1 is literally an
OR operation, though its result is equivalent to replacing the low byte with
`0xff`.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be an
externally consumed SerDes control API.

## Evidence

- Complete ARM64 body at `0x1d1c` through `0x1e24`.
- Exact compare chain, RMW masks and constants, strings, and common tail call.
- IDA type at `0x1d1c` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
