# 0x043ec serdes_get_hard_prbs_cnt

## Status

- Status: complete
- Confidence: verified setup-call ordering, 32-bit delay-count arithmetic,
  busy-wait loop order and constant, 64-bit counter accumulation, logging
  return, and export context.
- Size: `0xa4` bytes, 39 ARM64 instructions.
- Recovered signature: `int serdes_get_hard_prbs_cnt(uint32_t seconds, uint32_t prbs_mode)`.

## Semantics

Runs a synchronous hard PRBS measurement:

1. Resets the SerDes error counter.
2. Programs the error interval from `seconds`.
3. Selects the RX PRBS mode from `prbs_mode`.
4. Enables PRBS checking, error counting, and error-time control, in that order.
5. Executes `seconds * 1000` fixed-delay iterations.
6. Reads the error counter, adds it to the 64-bit global `iPrbsCounter`, and
   logs the accumulated total.

The multiplication uses a 32-bit `MUL W19, W19, #1000` result. Thus the loop
bound is `((uint32_t)(seconds * 1000U))`, not an unbounded 64-bit product.
The loop increments first, then calls `__const_udelay(0x418958UL)`. If the
truncated product is zero, it performs no delay calls.

## State and Concurrency

`iPrbsCounter @ 0x27870` is an inferred 64-bit global. It is read, incremented
by `serdes_get_err_cnt()`, and written back without locking or atomic
operations. Concurrent measurements can lose increments, and the busy-wait
loop can block its caller for a long time.

The vendor format string uses `%ld` while the operation is a raw 64-bit add;
the reconstruction casts the counter to `long` only to match the binary's
argument width and format representation.

## Return Semantics

Returns the result of the final `printk` call.

## Caller Context

There are no direct code or data xrefs to this entry in the current IDB. Vendor
`system/proc/kallsyms` contains `__ksymtab_serdes_get_hard_prbs_cnt` and global
text symbol `serdes_get_hard_prbs_cnt [plat_132]`, confirming it is exported.

## Evidence

- Complete ARM64 body at `0x43ec` through `0x448c`.
- Setup calls occur in exact order at `0x4404`, `0x4414`, `0x4420`, `0x4428`,
  `0x4430`, and `0x4438`.
- `MUL W19, W19, W0` at `0x4440` establishes the wrapping 32-bit bound.
- Loop body at `0x4444`-`0x4458` increments `X20` before invoking
  `__const_udelay` with `0x418958`.
- 64-bit `LDR X1`, `ADD X1, X0, X1`, `STR X1` at `0x4468`-`0x4478` updates
  `iPrbsCounter`.
- IDA type at `0x43ec` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
