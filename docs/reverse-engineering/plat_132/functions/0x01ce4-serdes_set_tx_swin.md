# 0x01ce4 serdes_set_tx_swin

## Status

- Status: complete
- Confidence: verified SerDes offset/field, unmasked input shift, RMW, log,
  return, and absence of direct code xrefs.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int serdes_set_tx_swin(u32 value)`.

## Semantics

Clears bits 16-17 at SerDes offset `0x20`, ORs the complete input shifted left
by 16, then returns the logging result. Inputs above two bits can alter higher
register bits.

## Caller Context

No direct code xrefs target this entry; it may be an external SerDes tuning API.

## Evidence

- Complete ARM64 body at `0x1ce4` through `0x1d18`.
- Exact offset, mask, unmasked shift, string, and tail return.
- IDA type at `0x1ce4` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
