# 0x1c400 dg_timer_init

## Status

- Status: complete
- Confidence: verified timer initialization call, callback, expires offset,
  delay, add result, and both direct callers.
- Size: `0x58` bytes, 20 ARM64 instructions.
- Recovered signature: `int dg_timer_init(void)`.

## Semantics

The helper initializes `dg_timer` with `dg_timer_func` and zero auxiliary
arguments, writes its word at offset `0x10` to `jiffies + 500`, then returns
the direct result of `add_timer(&dg_timer)`.

## Caller Context

`zx_pon_int @ 0x12bc` has two direct call sites, both scheduling this timer as
part of PON interrupt handling.

## Concurrency and Ownership

No local synchronization checks whether the timer is already pending. It always
reinitializes and adds the shared timer object.

## Evidence

- Complete ARM64 body at `0x1c400` through `0x1c454`.
- Exact `init_timer_key` arguments, `jiffies + 0x1f4`, object offset `0x10`,
  and tail return from `add_timer`.
- Exhaustive direct xref query found two `zx_pon_int` callers.
- IDA type at `0x1c400` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Whether the callers guarantee the shared timer is not already pending.
