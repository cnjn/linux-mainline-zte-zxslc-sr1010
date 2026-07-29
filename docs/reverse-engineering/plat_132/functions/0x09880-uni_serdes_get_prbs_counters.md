# 0x09880 uni_serdes_get_prbs_counters

## Status

- Status: complete
- Confidence: verified ordered timer setup, otherwise-unused volatile status
  read, 32-bit expiry arithmetic, baseline capture, final zero return, and
  exported ABI.
- Size: `0xc0` bytes, 46 ARM64 instructions.
- Recovered signature: `int uni_serdes_get_prbs_counters(int time_units)`.

## Semantics

Schedules the Uni SerDes PRBS counter-reporting callback after a requested
interval:

1. Logs `time_units` and disables error-time enable.
2. Reads `uni_serdes_base + 0xe4` as a volatile 32-bit access whose result is
   discarded.
3. Cancels and reinitializes `uni_serdes_prbs_counter_timer` with
   `uni_serdesPrbsCounterGetHandler`.
4. Calculates `(uint32_t)time_units * 100`, adds it to `jiffies`, and stores
   the result in the timer expiry field before adding the timer.
5. Resets the hardware error counter, delays once with
   `__const_udelay(0x418958)`, stores a fresh 48-bit counter baseline in
   `uni_serdesPrbsCounter`, and returns zero.

The tick product intentionally uses 32-bit multiplication, matching `MUL W19,
W19, W1` in the binary before zero extension for the `jiffies` addition.

## Caller Context

No internal IDB xrefs target this exported entry. It is exported through
`__ksymtab_uni_serdes_get_prbs_counters`. The initialized callback is
`uni_serdesPrbsCounterGetHandler @ 0x87fc`.

## Evidence

- Complete ARM64 body at `0x9880` through `0x993c`.
- Error-time disable at `0x98bc`-`0x98c0`; retained volatile `+0xe4` load at
  `0x98c4`-`0x98cc`.
- Timer delete at `0x98d0`-`0x98d4`, callback setup at `0x98d8`-`0x98f0`, and
  add at `0x9908`-`0x9910`.
- 32-bit tick product at `0x98f8`-`0x98fc`; expiry store at `0x990c` to
  `uni_serdes_prbs_counter_timer + 0x10` (`0x27900`).
- Counter reset, fixed delay, and baseline store occur at `0x9914`, `0x9920`,
  and `0x9928`.
- IDA type at `0x9880` set to the recovered signature and Hex-Rays cache
  invalidated after verification. Instruction comments at `0x98cc` and
  `0x990c` preserve the omitted volatile read and the timer-expiry arithmetic.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
