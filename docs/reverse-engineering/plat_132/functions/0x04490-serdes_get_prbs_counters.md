# 0x04490 serdes_get_prbs_counters

## Status

- Status: complete
- Confidence: verified diagnostic prefix, error-time disable ordering, LOS
  branch, timer lifecycle, wrapping expiry calculation, PRBS baseline sample,
  constant-zero return, and export context.
- Size: `0xd0` bytes, 49 ARM64 instructions.
- Recovered signature: `int serdes_get_prbs_counters(int time)`.

## Semantics

This exported entry prepares timer-based PRBS counting rather than returning an
immediate count:

1. Logs `serdes_get_prbs_counters` and the supplied `time`.
2. Disables error-time control with `serdes_set_error_time_en(0)`.
3. If `los_state_prbs != 0`, logs that RXBIST is not locked and returns 0.
4. Otherwise, deletes and reinitializes `serdes_prbs_counter_timer` with
   `serdesPrbsCounterGetHandler`.
5. Sets the timer expiry to `jiffies + (uint32_t)(time * 100U)`, adds the timer,
   resets the error counter, waits once with `__const_udelay(0x418958UL)`, and
   stores a fresh 64-bit error-counter baseline in `serdesPrbsCounter`.

The expiry multiplication is a 32-bit `MUL W19, W19, #100`. A negative
`time` or an overflowing product is interpreted as its wrapped 32-bit bit
pattern, then zero-extended before adding to `jiffies`.

## State and Concurrency

The timer expiration field is the qword at
`serdes_prbs_counter_timer + 0x10`, address `0x27888`, represented as
`recovered_timer_t.expires`. The routine calls `del_timer`, not an explicitly
synchronizing variant, and has no local locking around timer state or
`serdesPrbsCounter`; the binary itself provides no serialization guarantee.

## Return Semantics

Every path converges on `MOV W0, #0` at `0x454c`. All helper and `printk`
results are discarded, so the recovered function always returns 0.

## Caller Context

There are no direct code or data xrefs to this entry in the current IDB. Vendor
`system/proc/kallsyms` contains `__ksymtab_serdes_get_prbs_counters` and global
text symbol `serdes_get_prbs_counters [plat_132]`, confirming it is exported.

## Evidence

- Complete ARM64 body at `0x4490` through `0x455c`.
- Error-time disable and LOS branch at `0x44c0`-`0x44d0`.
- Timer deletion, initialization, expiration store, and addition at
  `0x44e0`-`0x4520`.
- `MUL W19, W19, W1` at `0x450c`, followed by a 64-bit add to `jiffies`.
- Baseline reset/sample at `0x4524`-`0x4538`.
- Both branch exits converge on `MOV W0, #0` at `0x454c`.
- IDA type at `0x4490` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
