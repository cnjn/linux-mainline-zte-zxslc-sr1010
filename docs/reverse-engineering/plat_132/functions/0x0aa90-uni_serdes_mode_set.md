# 0x0aa90 uni_serdes_mode_set

## Status

- Status: complete
- Confidence: verified unsigned mode bound, every jump-table case group,
  callee mapping, caller context, and semantic void ABI.
- Size: `0x58` bytes, 22 ARM64 instructions.
- Recovered signature: `void uni_serdes_mode_set(uint32_t mode)`.

## Semantics

Dispatches Uni SerDes modes zero through eight to a profile writer:

| Modes | Callee |
| --- | --- |
| 0, 1 | `uni_mode_eth_10gbase_r_cfg` |
| 2, 3 | `uni_mode_eth_5gbase_r_cfg` |
| 4 | `uni_mode_eth_2p5gbase_r_cfg` |
| 5, 6 | `uni_mode_eth_2p5gbase_x_cfg` |
| 7, 8 | `uni_mode_eth_1gbase_x_cfg` |
| all other unsigned values | no operation |

The initial unsigned `mode > 8` branch returns before allocating a frame or
calling any profile. Profile residual registers are not semantic return values.

## Caller Context

`uni_zx_serdes_init @ 0xaae8` is the sole direct caller at `0xab00`. The
function is local text (`t`) in runtime `kallsyms`.

## Evidence

- Complete ARM64 body at `0xaa90` through `0xaae4`.
- Unsigned range gate at `0xaa90`-`0xaa94`; byte jump table at
  `0xaa98`-`0xaab4`.
- Call targets at `0xaab8`, `0xaac0`, `0xaac8`, `0xaad0`, and `0xaad8` encode
  the mode groups above.
- `uni_zx_serdes_init` calls it at `0xab00`.
- IDA type at `0xaa90` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
