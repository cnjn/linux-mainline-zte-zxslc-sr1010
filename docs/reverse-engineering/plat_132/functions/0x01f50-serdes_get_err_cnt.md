# 0x01f50 serdes_get_err_cnt

## Status

- Status: complete
- Confidence: verified register read order, 48-bit composition, callers, log,
  and unsigned 64-bit return.
- Size: `0x48` bytes, 18 ARM64 instructions.
- Recovered signature: `u64 serdes_get_err_cnt(void)`.

## Semantics

Reads the low 32 bits of a PRBS error count from SerDes offset `0xe8`, then
reads offset `0xec` and uses only its low 16 bits as count bits 32-47. It logs
and returns the resulting unsigned 48-bit value in a 64-bit container.

The binary reads the low word before the high half-word and contains no retry
or explicit snapshot operation in this helper.

## Caller Context

- `serdesPrbsCounterGetHandler @ 0x2048` subtracts a saved baseline when the
  current count has not wrapped.
- `serdes_get_hard_prbs_cnt @ 0x43ec` adds the value to `iPrbsCounter`.
- `serdes_get_prbs_counters @ 0x4490` stores a baseline after resetting and
  delaying.

## Evidence

- Complete ARM64 body at `0x1f50` through `0x1f94`.
- `LDR W19, [X0,#0xE8]`, `LDR W0, [X0,#0xEC]`, then
  `UBFIZ X0, X0, #0x20, #0x10` and `ORR X19, X0, X19`.
- Three direct call xrefs and their decompiled uses.
- IDA type at `0x1f50` updated to the recovered unsigned 64-bit signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
