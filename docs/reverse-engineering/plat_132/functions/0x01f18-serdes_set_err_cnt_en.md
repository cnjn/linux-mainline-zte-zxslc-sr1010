# 0x01f18 serdes_set_err_cnt_en

## Status

- Status: complete
- Confidence: verified SerDes offset/field, unmasked input, RMW, callers, log,
  and return behavior.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int serdes_set_err_cnt_en(u32 enable)`.

## Semantics

Clears bit 15 at SerDes offset `0x94`, ORs the complete `enable` input shifted
left by 15, and returns the logging result. Inputs above one bit can alter
higher register fields.

## Caller Context

`serdes_set_sprbsrxbist @ 0x2f90` forwards its RX-BIST enable argument to this
helper. `serdes_get_hard_prbs_cnt @ 0x43ec` calls it with 1 immediately after
enabling the PRBS checker.

## Evidence

- Complete ARM64 body at `0x1f18` through `0x1f4c`.
- Exact offset, mask, unmasked shift, log string, and tail return.
- Direct call xrefs at `0x2fb8` and `0x4430` and their decompiled arguments.
- IDA type at `0x1f18` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
