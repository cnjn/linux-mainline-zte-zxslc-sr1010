# 0x11c64 greg_init_done_check

## Status

- Status: complete
- Confidence: verified readiness predicate, exact retry count/delay, logs, and
  return values.
- Size: `0x84` bytes, 33 ARM64 instructions.
- Recovered signature: `int greg_init_done_check(void)`.

## Semantics

Polls `nppt_base + 0x80` until `(value & 0x1fd) == 0x1fd`. Success logs the
number of prior failures and returns zero. Every failed poll increments the
counter before `__const_udelay(429500)`; after exactly 400 failed iterations it
logs failure and returns `-1`. No lock, barrier, or timeout recovery occurs.

## Caller Context

`zx_pon_probe` propagates a nonzero return and reaches SDET restore only after
this helper succeeds.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
