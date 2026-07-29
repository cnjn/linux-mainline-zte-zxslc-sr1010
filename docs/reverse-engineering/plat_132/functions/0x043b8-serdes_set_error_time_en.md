# 0x043b8 serdes_set_error_time_en

## Status

- Status: complete
- Confidence: verified the full RMW, unmasked shifted input, module-local
  callers, logging return, and symbol visibility.
- Size: `0x34` bytes, 12 ARM64 instructions.
- Recovered signature: `int serdes_set_error_time_en(uint32_t enable)`.

## Semantics

Reads `pon_serdes_base + 0x94`, clears bit 30 with `0xbfffffff`, ORs
`enable << 30` without masking the input, writes the result back, and logs
`set error time is ok`.

For the observed in-module calls, input 1 sets bit 30 and input 0 clears it.
Because the input is not constrained before its 32-bit left shift, values with
additional low bits can also affect bit 31 after the shift; that behavior is
preserved.

## Return Semantics

The result of the final `printk` is returned directly.

## Caller Context

Two direct code callers exist:

| Caller | Input | Purpose |
| --- | --- | --- |
| `serdes_get_hard_prbs_cnt @ 0x43ec` | 1 | Enables error-time behavior before hard PRBS sampling. |
| `serdes_get_prbs_counters @ 0x4490` | 0 | Disables it before timer-based PRBS counting. |

Unlike the preceding setters, this function's symbol is lowercase `t` in the
vendor kallsyms and has no `__ksymtab_serdes_set_error_time_en` entry. The
direct function is therefore module-private; a distinct `uni_` symbol is
exported elsewhere.

## Evidence

- Complete ARM64 body at `0x43b8` through `0x43e8`.
- `LDR` / `AND #0xbfffffff` / `ORR W0, W1, W0,LSL#30` / `STR` at
  `0x43c8`-`0x43d4`.
- Shared `printk` result reaches `RET` unchanged.
- The two callers pass literal 1 and 0 at `0x4438` and `0x44c4`.
- IDA type at `0x43b8` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
