# 0x02874 serdes_set_rx_eq_mbf

## Status

- Status: complete
- Confidence: verified SerDes offset/field, unmasked input, RMW, log, return,
  and absence of direct xrefs.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int serdes_set_rx_eq_mbf(u32 value)`.

## Semantics

Clears bits 18-21 at SerDes offset `0x2c`, ORs the complete input shifted left
by 18, and returns the logging result. Values above four bits can alter higher
register fields.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be an
externally consumed receive-equalizer tuning API.

## Evidence

- Complete ARM64 body at `0x2874` through `0x28a8`.
- Exact offset, `0xffc3ffff` mask, unmasked shift, string, and tail return.
- IDA type at `0x2874` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
