# 0x01824 serdes_err_cnt_reset

## Status

- Status: complete
- Confidence: verified SerDes offset, clear/set pulse, separate reread, zero
  return, and both direct callers.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature: `int serdes_err_cnt_reset(void)`.

## Semantics

At PON SerDes offset `0x94`, clears bit 15, rereads the word, then sets bit 15.
Returns zero.

## Caller Context

Direct callers are `serdes_get_hard_prbs_cnt @ 0x43ec` and
`serdes_get_prbs_counters @ 0x4490`.

## Evidence

- Complete ARM64 body at `0x1824` through `0x1848`.
- Exact offset, clear/set masks, separate loads, and caller xrefs.
- IDA type at `0x1824` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
