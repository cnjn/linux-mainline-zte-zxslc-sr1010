# 0x07cc0 serdes_mode_set

## Status

- Status: complete
- Confidence: verified full 17-entry jump table, every direct profile edge,
  shared cases, default behavior, caller context, and semantic void ABI.
- Size: `0x98` bytes, 38 ARM64 instructions.
- Recovered signature: `void serdes_mode_set(uint32_t mode)`.

## Semantics

Dispatches `mode` values 0 through 16 to PON and Ethernet SerDes profile
functions. Values above 16 execute the default return and perform no work.

| Mode | Profile callee |
| --- | --- |
| 0 | `mode_epon_cfg` |
| 1 | `mode_10g_epon_nsyn_dpll_cfg` |
| 2 | `mode_10g_epon_nsyn_fifo_cfg` |
| 3 | `mode_10g_epon_nsyn_nofifo_cfg` |
| 4 | `mode_10g_epon_syn_cfg` |
| 5 | `mode_gpon_cfg` |
| 6 | `mode_xgpon_nsyn_cfg` |
| 7 | `mode_xgpon_syn_cfg` |
| 8, 16 | `mode_eth_1gbase_x_cfg` |
| 9, 15 | `mode_eth_2p5gbase_x_cfg` |
| 10 | `mode_eth_2p5gbase_r_cfg` |
| 11, 12 | `mode_eth_5gbase_r_cfg` |
| 13, 14 | `mode_eth_10gbase_r_cfg` |

The binary passes two generic registers to some callees, but the profile bodies
do not consume them. The source therefore retains only the semantic mode input.

## Return Semantics

The original leaves either the profile's incidental register residual or the
unsupported selector in `x0`; `pon_serdes_init` discards it. The recovered ABI
is `void`.

## Caller Context

`pon_serdes_init @ 0x7d58` is the sole direct caller. It runs this dispatcher
after the CPU-132-only AN1 PLL setup, then performs common SerDes post-setup and
lock polling.

## Evidence

- Complete ARM64 body at `0x7cc0` through `0x7d54`.
- Unsigned bounds check at `0x7cc0`-`0x7cc4`, byte jump table at `0x1dbc4`, and
  default `RET` at `0x7d54`.
- Direct calls at `0x7ce8`, `0x7cf0`, `0x7cf8`, `0x7d00`, `0x7d08`, `0x7d10`,
  `0x7d18`, `0x7d20`, `0x7d28`, `0x7d30`, `0x7d38`, `0x7d40`, and `0x7d48`.
- Sole caller at `0x7d88`.
- IDA type at `0x7cc0` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
