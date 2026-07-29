# 0x064c0 an1_pll_epon_cfg

## Status

- Status: complete
- Confidence: verified fixed AN1 PLL register sequence, enable RMW, bounded
  lock polling, EPON logs, caller context, and `int` ABI.
- Size: `0xd4` bytes, 47 ARM64 instructions.
- Recovered signature: `int an1_pll_epon_cfg(void)`.

## Semantics

Programs the same eight 32-bit AN1 PLL words as `eth_an1_clk_set` at
`pon_serdes_pll_base + 0x00..0x1c`, then reads offset `0x10`, ORs bit zero, and
writes it back.

It starts a counter at 1001 and polls bit zero at offset `0x20`. Every failed
poll calls `__const_udelay(0x418958)`, then decrements the counter. On
exhaustion it logs `epon AN1_pll_lock failed!`; both timeout and success log
`epon AN1_pll_lock_finish` and return the final `printk` result.

## Caller Context

`an1_pll_clk_set @ 0x7c74` is the sole direct caller. It selects this entry for
mode values 0 through 4; `pon_serdes_init @ 0x7d58` invokes that dispatcher on
CPU type 132 and ignores its result.

## Evidence

- Complete ARM64 body at `0x64c0` through `0x6590`.
- Literal profile stores at `0x64f0`-`0x6540`; enable RMW at
  `0x6544`-`0x654c`.
- Lock read at `0x6554`, retry counter 1001 at `0x64e8`, and delay value
  `0x418958` assembled at `0x64e4`-`0x64ec`.
- Timeout/final log control flow at `0x656c`-`0x6580`.
- The sole direct call is `an1_pll_clk_set` at `0x7ca4`.
- IDA type at `0x64c0` set to the recovered `int` signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
