# 0x063ec eth_an1_clk_set

## Status

- Status: complete
- Confidence: verified fixed AN1 PLL register sequence, enable RMW, bounded
  lock polling, log/return behavior, caller context, and `int` ABI.
- Size: `0xd4` bytes, 47 ARM64 instructions.
- Recovered signature: `int eth_an1_clk_set(void)`.

## Semantics

Programs eight 32-bit AN1 PLL words at `pon_serdes_pll_base + 0x00..0x1c`:
`0x2af8c074`, `0x447424a0`, `0x01050707`, `0x107`, `0x3100`, `0x4000003a`,
`0x2`, and `0x130200`. It then reads offset `0x10`, ORs bit zero, and writes
the result back.

It starts a counter at 1001 and polls bit zero at offset `0x20`. Every failed
poll delays with `__const_udelay(0x8312b0)`, then decrements the counter. On
exhaustion it logs `eth AN1_pll_lock failed!`, always logs
`eth AN1_pll_lock_finish`, and returns the final `printk` result. A successful
poll skips the failure log and returns that same final `printk` result.

## Caller Context

`an1_pll_clk_set @ 0x7c74` is the sole direct caller. For mode values 8 through
16 it selects this Ethernet AN1 PLL sequence; `pon_serdes_init @ 0x7d58` calls
that dispatcher only on CPU type 132 and discards its return.

## Evidence

- Complete ARM64 body at `0x63ec` through `0x64bc`.
- Literal profile stores at `0x641c`-`0x646c`; the enable RMW is
  `0x6470`-`0x6478`.
- Lock read at `0x6480`, initial retry value 1001 at `0x6414`, and delay value
  `0x8312b0` assembled at `0x6410`-`0x6418`.
- Timeout/final log control flow at `0x6498`-`0x64ac`.
- The sole direct call is `an1_pll_clk_set` at `0x7cb4`.
- IDA type at `0x63ec` set to the recovered `int` signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
