# 0x0a354 uni_mode_eth_2p5gbase_x_cfg

## Status

- Status: complete
- Confidence: verified CPU-133/129 predicate order, both 49-word profiles,
  branch-specific `+0xac` values, common tail, and semantic void ABI.
- Size: `0x3a8` bytes, 194 ARM64 instructions.
- Recovered signature: `void uni_mode_eth_2p5gbase_x_cfg(void)`.

## Semantics

Logs `mode_eth_2p5gbase_x_cfg`, then selects a Uni 2.5GBASE-X profile:

| CPU predicate | Profile |
| --- | --- |
| `isCpuType_133() == 1` | 49 ordered 32-bit stores at `0x00..0xc0`, with `+0xac = 0x40002000` |
| otherwise, `isCpuType_129() == 1` | distinct 49-word profile, with `+0xac = 0x0000201c` |
| neither | log only; no MMIO access |

Both supported paths write profile words through `+0xac`, then share five
ordered 32-bit tail stores at `+0xb0..+0xc0`. No profile access reads or RMWs a
Uni SerDes register. The CPU-133 check has priority; CPU 129 is not queried
when it returns exactly one.

## Return Semantics

The logging result and successful-path base-pointer residues do not define an
API result. The recovered semantic ABI is `void`.

## Caller Context

`uni_serdes_mode_set @ 0xaa90` is the sole internal caller, for mode values
five and six. The function is local text (`t`) in runtime `kallsyms`.

## Evidence

- Complete ARM64 body at `0xa354` through `0xa6f8`.
- CPU-133 gate at `0xa368`-`0xa370`; CPU-129 fallback gate at
  `0xa540`-`0xa548`.
- CPU-133 profile stores at `0xa38c`-`0xa530`; CPU-129 stores at
  `0xa55c`-`0xa6cc`.
- Branch-specific `+0xac` values are prepared at `0xa534` and `0xa6d0`; common
  tail stores begin at `0xa6d4`.
- `uni_serdes_mode_set` calls this profile at `0xaad0` for jump-table cases
  five and six.
- Multiple apparent 64-bit/string operations are actually separate 32-bit
  stores, including `+0x58/+0x5c`, `+0xa0/+0xa4`, and `+0xbc/+0xc0`.
  Instruction comments at `0xa470`, `0xa518`, `0xa614`, `0xa6b4`, and `0xa6ec`
  preserve those non-merge constraints in IDA.
- IDA type at `0xa354` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
