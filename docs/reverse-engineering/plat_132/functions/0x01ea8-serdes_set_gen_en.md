# 0x01ea8 serdes_set_gen_en

## Status

- Status: complete
- Confidence: verified SerDes offset/field, unmasked input, RMW, callers, log,
  and return behavior.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int serdes_set_gen_en(u32 enable)`.

## Semantics

Clears bit 13 at SerDes offset `0x94`, ORs the complete `enable` input shifted
left by 13, and returns the logging result. Inputs above one bit can alter
higher register fields.

## Caller Context

`serdes_set_tx_prbs_mode @ 0x2bf0` is the only direct caller. It passes 1 at
entry to enable the PRBS generator and passes 0 in mode 5 before loading the
fixed `0101` pattern registers.

## Evidence

- Complete ARM64 body at `0x1ea8` through `0x1edc`.
- Exact offset, mask, unmasked shift, log string, and tail return.
- Direct call xrefs at `0x2c04` and `0x2d08` with values 1 and 0.
- IDA type at `0x1ea8` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
