# 0x0a6fc uni_mode_eth_1gbase_x_cfg

## Status

- Status: complete
- Confidence: verified CPU-133/129 predicate order, both 49-word profiles,
  branch-specific `+0xac` values, common tail, and semantic void ABI.
- Size: `0x394` bytes, 193 ARM64 instructions.
- Recovered signature: `void uni_mode_eth_1gbase_x_cfg(void)`.

## Semantics

Logs `mode_eth_1gbase_x_cfg`, then selects a Uni 1GBASE-X profile:

| CPU predicate | Profile |
| --- | --- |
| `isCpuType_133() == 1` | 49 ordered 32-bit stores at `0x00..0xc0`, with `+0xac = 0x40002000` |
| otherwise, `isCpuType_129() == 1` | distinct 49-word profile, with `+0xac = 0x0000201c` |
| neither | log only; no MMIO access |

The CPU-133 predicate has priority. Both supported paths write through `+0xac`,

## Return Semantics

Logging and successful-path residual values do not define an interface result;
the recovered semantic ABI is `void`.

## Caller Context

`uni_serdes_mode_set @ 0xaa90` is the sole internal caller, for mode values
seven and eight. The function is local text (`t`) in runtime `kallsyms`.

## Evidence

- Complete ARM64 body at `0xa6fc` through `0xaa8c`.
- CPU-133 gate at `0xa710`-`0xa718`; CPU-129 fallback at
  `0xa8d8`-`0xa8e0`.
- CPU-133 profile stores at `0xa730`-`0xa8c8`; CPU-129 stores at
  `0xa8f4`-`0xaa60`.
- Branch-specific `+0xac` values are prepared at `0xa8cc` and `0xaa64`; common
  tail stores begin at `0xaa68`.
- `uni_serdes_mode_set` calls this profile at `0xaad8` for jump-table cases
  seven and eight.
- Multiple apparent 64-bit/string operations are actually separate 32-bit
  stores, including `+0x58/+0x5c`, `+0xa0/+0xa4`, and `+0xbc/+0xc0`.
  Instruction comments at `0xa808`, `0xa8b0`, `0xa9a8`, `0xaa48`, and `0xaa80`
  preserve those non-merge constraints in IDA.
- IDA type at `0xa6fc` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
