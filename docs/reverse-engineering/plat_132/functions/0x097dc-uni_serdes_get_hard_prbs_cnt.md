# 0x097dc uni_serdes_get_hard_prbs_cnt

## Status

- Status: complete
- Confidence: verified ordered PRBS setup calls, 32-bit delay-count arithmetic,
  delay loop, persistent counter accumulation, final log return, and exported
  ABI.
- Size: `0xa4` bytes, 39 ARM64 instructions.
- Recovered signature:
  `int uni_serdes_get_hard_prbs_cnt(uint32_t time_units, int prbs_mode)`.

## Semantics

Performs a hard Uni SerDes PRBS count transaction:

1. Resets the error counter.
2. Programs error time from `time_units`.
3. Programs RX PRBS mode and enables checker, error counter, and error time.
4. Executes `time_units * 1000` delay calls using 32-bit multiplication and
   `__const_udelay(0x418958)` for each iteration.
5. Reads the 48-bit error counter, adds it to persistent `uni_iPrbsCounter`,
   logs the accumulated value, and returns the final `printk` result.

The multiplication is intentionally 32-bit, so large `time_units` values wrap
before determining the iteration count.

## Caller Context

No internal IDB xrefs target this exported entry. It is exported through
`__ksymtab_uni_serdes_get_hard_prbs_cnt`.

## Evidence

- Complete ARM64 body at `0x97dc` through `0x987c`.
- Setup calls at `0x97f4`, `0x9804`, `0x9810`, `0x9818`, `0x9820`, and `0x9828`.
- 32-bit multiply by 1000 at `0x982c`-`0x9830`; delay loop at
  `0x9834`-`0x9848` with `0x418958` loop argument.
- Error-counter read at `0x984c`; persistent accumulation/store at
  `0x9858`-`0x9868`; final returned log at `0x986c`.
- IDA type at `0x97dc` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
