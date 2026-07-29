# 0x087fc uni_serdesPrbsCounterGetHandler

## Status

- Status: complete
- Confidence: verified counter getter call, unsigned baseline comparison,
  underflow message, data-reference context, and `int(void)` ABI.
- Size: `0x44` bytes, 15 ARM64 instructions.
- Recovered signature: `int uni_serdesPrbsCounterGetHandler(void)`.

## Semantics

Reads the current Uni SerDes 48-bit error counter. If it is at least
`uni_serdesPrbsCounter`, logs and returns the difference. If the current count
is lower, logs an overflow-detected error and returns that log result. The
function does not update the baseline counter itself.

## Caller Context

`uni_serdes_get_prbs_counters @ 0x9880` holds two data references to this entry
at `0x98d8` and `0x98e0`, consistent with retained callback/function-pointer
context.

## Evidence

- Complete ARM64 body at `0x87fc` through `0x883c`.
- Error-counter getter call at `0x8804`; baseline load at `0x880c`.
- Unsigned comparison at `0x8810`/`0x8814`; underflow log at `0x8820` and
  difference calculation/log at `0x8828`-`0x8834`.
- IDA type at `0x87fc` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
