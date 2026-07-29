# 0x07c74 an1_pll_clk_set

## Status

- Status: complete
- Confidence: verified selector bounds, raw mask grouping, all callee edges,
  caller context, and semantic `int` ABI.
- Size: `0x4c` bytes, 19 ARM64 instructions.
- Recovered signature: `int an1_pll_clk_set(uint32_t mode)`.

## Semantics

Accepts a mode selector. Selectors above 16 trigger no child call and return
their low 32-bit residual. For selectors 0 through 16 it constructs
`1ULL << mode` and uses the following masks:

| Selector range | Mask | Callee |
| --- | --- | --- |
| 0-4 | `0x1f` | `an1_pll_epon_cfg()` |
| 5-7 | `0xe0` | `an1_pll_gpon_cfg()` |
| 8-16 | `0x1ff00` | `eth_an1_clk_set()` |

The selected child return is propagated unchanged. The source retains the raw
mask logic because it directly reflects the ARM64 shift/test sequence.

## Caller Context

`pon_serdes_init @ 0x7d58` is the sole direct caller. It invokes this dispatcher
only on CPU type 132, then discards the return before configuring the PON SerDes
mode profile.

## Evidence

- Complete ARM64 body at `0x7c74` through `0x7cbc`.
- Unsigned selector bound check at `0x7c78`-`0x7c80`.
- Mode-bit construction at `0x7c84`-`0x7c88`; masks `0x1ff00`, `0xe0`, and
  `0x1f` tested at `0x7c8c`, `0x7c94`, and `0x7c9c`.
- Direct child calls: EPON at `0x7ca4`, GPON at `0x7cac`, Ethernet at `0x7cb4`.
- Sole caller at `0x7d7c`.
- IDA type at `0x7c74` set to the recovered semantic signature and Hex-Rays
  cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
