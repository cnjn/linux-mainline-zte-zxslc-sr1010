# 0x02048 serdesPrbsCounterGetHandler

## Status

- Status: complete
- Confidence: verified timer-callback use, counter comparison and subtraction,
  both log paths, global baseline, and semantic void return.
- Size: `0x44` bytes, 15 ARM64 instructions.
- Recovered signature: `void serdesPrbsCounterGetHandler(void)`.

## Semantics

Reads the current 48-bit PRBS error count with `serdes_get_err_cnt`. If the
current value is below the saved `serdesPrbsCounter` baseline, it logs an
overflow error. Otherwise it logs `current - baseline`.

The callback does not update the baseline or rearm the timer. Although the
called `printk` leaves its result in `x0`, this entry is installed as a timer
callback, so that residual register value is not a semantic function return.

## Caller Context

There are no direct code-call xrefs. `serdes_get_prbs_counters @ 0x4490` takes
this entry's address and passes it to `init_timer_key` for the global
`serdes_prbs_counter_timer`.

## Evidence

- Complete ARM64 body at `0x2048` through `0x2088`.
- Unsigned `CMP`/`B.CS` against the 64-bit `serdesPrbsCounter` global.
- Exact overflow and subtraction log paths.
- Two address-materialization xrefs from timer setup at `0x44e8`/`0x44f0`.
- IDA type at `0x2048` updated to the recovered void callback signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
