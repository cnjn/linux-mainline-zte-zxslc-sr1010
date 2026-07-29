# 0x07d58 pon_serdes_init

## Status

- Status: complete
- Confidence: verified CPU gates, AN1/profile dispatch, all RMWs, three lock
  loops, failure returns, caller context, and `int` ABI.
- Size: `0x1c8` bytes, 106 ARM64 instructions.
- Recovered signature: `int pon_serdes_init(uint32_t mode)`.

## Semantics

Initializes the PON SerDes for `mode`:

1. On CPU 132, calls `an1_pll_clk_set(mode)` and discards its result.
2. Calls `serdes_mode_set(mode)` on every CPU.
3. Performs three ordered RMWs: replace `pon_serdes_base + 0x90` bit 13 with
   one, set `+0x40` bit 15, then set `+0x54` bit zero.
4. For CPU 132 or 133, polls `+0xd0` bit zero for common PLL lock; CPU 129
   instead polls `+0xcc` bit one. Either loop starts at 1001 and delays with
   `__const_udelay(0x8312b0)` after failed reads.
5. Logs RX LOS from `+0xe4` bit zero, then polls `+0xe4` bit one for CDR lock.
6. CPU 129 additionally polls `+0xe4` bits 9 and 10 until either is set.

Each failed lock loop logs its specific message and returns `-1`; success
returns zero. An unsupported CPU skips the common PLL-lock loop but still runs
the LOS and CDR checks.

## Caller Context

`zx_pon_clk_reset_init @ 0x8088` is the sole direct caller. It invokes this
routine during PON clock/reset bring-up and receives its success/failure status.

## Evidence

- Complete ARM64 body at `0x7d58` through `0x7f1c`.
- CPU-132 AN1 call at `0x7d7c`, profile dispatch at `0x7d88`, and RMWs at
  `0x7d90`-`0x7db4`.
- CPU-132/133 PLL loop reads `+0xd0` at `0x7de0`; CPU-129 loop reads `+0xcc`
  at `0x7e18`; timeout converges at `0x7df8`.
- LOS read at `0x7e50`, CDR loop at `0x7e7c`-`0x7e94`, and CPU-129 extra
  bit-9/10 loop at `0x7ec8`-`0x7ee8`.
- The sole direct call is at `0x8138`.
- IDA type at `0x7d58` set to the recovered semantic signature and Hex-Rays
  cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
