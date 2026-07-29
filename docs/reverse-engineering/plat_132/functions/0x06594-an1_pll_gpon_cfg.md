# 0x06594 an1_pll_gpon_cfg

## Status

- Status: complete
- Confidence: verified GPON-specific PLL words, enable RMW, bounded lock
  polling, log/return behavior, caller context, and `int` ABI.
- Size: `0xd0` bytes, 47 ARM64 instructions.
- Recovered signature: `int an1_pll_gpon_cfg(void)`.

## Semantics

Programs an eight-word AN1 PLL profile at `pon_serdes_pll_base + 0x00..0x1c`.
It is the Ethernet/EPON profile except for `0x08=0x01050700` and
`0x1c=0x00130000`; all other words are `0x2af8c074`, `0x447424a0`, `0x107`,
`0x3100`, `0x4000003a`, and `0x2`. It then reads offset `0x10`, ORs bit zero,
and writes it back.

It starts a counter at 1001 and polls bit zero at offset `0x20`. Every failed
poll calls `__const_udelay(0x8312b0)`, then decrements the counter. On
exhaustion it logs `gpon AN1_pll_lock failed!`; both timeout and success log
`gpon AN1_pll_lock_finish` and return the final `printk` result.

## Caller Context

`an1_pll_clk_set @ 0x7c74` is the sole direct caller. It selects this entry for
mode values 5 through 7; `pon_serdes_init @ 0x7d58` invokes that dispatcher on
CPU type 132 and ignores its result.

## Evidence

- Complete ARM64 body at `0x6594` through `0x6660`.
- Literal profile stores at `0x65c4`-`0x6610`; enable RMW at
  `0x6614`-`0x661c`.
- Lock read at `0x6624`, retry counter 1001 at `0x65bc`, and delay value
  `0x8312b0` assembled at `0x65b8`-`0x65c0`.
- Timeout/final log control flow at `0x663c`-`0x6650`.
- The sole direct call is `an1_pll_clk_set` at `0x7cac`.
- IDA type at `0x6594` set to the recovered `int` signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
