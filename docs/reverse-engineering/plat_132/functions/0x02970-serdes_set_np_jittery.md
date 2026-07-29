# 0x02970 serdes_set_np_jittery

## Status

- Status: complete
- Confidence: verified SerDes offset/field, unmasked input, RMW, log, return,
  and absence of direct xrefs.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int serdes_set_np_jittery(u32 value)`.

## Semantics

Clears bits 6-8 at SerDes offset `0x48`, ORs the complete input shifted left
by 6, and returns the logging result. Values above three bits can alter higher
register fields.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be an
externally consumed SerDes tuning API.

## Evidence

- Complete ARM64 body at `0x2970` through `0x29a4`.
- Exact offset, `0xfffffe3f` mask, unmasked shift, string, and tail return.
- IDA type at `0x2970` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
