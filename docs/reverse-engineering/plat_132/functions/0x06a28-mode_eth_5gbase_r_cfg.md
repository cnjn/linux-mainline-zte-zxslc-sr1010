# 0x06a28 mode_eth_5gbase_r_cfg

## Status

- Status: complete
- Confidence: verified both CPU profiles, all ordered 32-bit stores, common
  tail, dual dispatcher-case context, and semantic void ABI.
- Size: `0x3c8` bytes, 196 ARM64 instructions.
- Recovered signature: `void mode_eth_5gbase_r_cfg(void)`.

## Semantics

Logs `mode_eth_5gbase_r_cfg`, then selects an Ethernet 5GBASE-R SerDes profile.
CPU 132 has priority; CPU 133 is considered only if the 132 predicate does not
return exactly one.

| CPU predicate | Profile writes | Shared tail |
| --- | --- | --- |
| `isCpuType_132() == 1` | 46 ordered 32-bit stores at `0x00..0xb4` | `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0` |
| otherwise, `isCpuType_133() == 1` | 46 different ordered 32-bit stores at `0x00..0xb4` | same three stores |
| neither | no MMIO writes | none |

The dispatcher arguments are unused. This is an ordered 32-bit write-only
script; it performs no SerDes reads or RMWs.

## Return Semantics

The supported paths retain a base-pointer residual and the unsupported path
retains the CPU-133 predicate result. Neither is semantic, so the recovered ABI
is `void`.

## Caller Context

`serdes_mode_set @ 0x7cc0` is the sole direct caller. Cases 11 and 12 share
this entry; `check_serdes_config @ 0x2a58` labels case 12
`MODE_ETH_USXGMII_5G` but reaches its default diagnostic label for case 11.

## Evidence

- Complete ARM64 body at `0x6a28` through `0x6dec`.
- CPU-132 gate at `0x6a3c`-`0x6a44`; CPU-133 fallback at
  `0x6bf8`-`0x6c00`.
- CPU-132 stores at `0x6a60`-`0x6bf0`; CPU-133 stores at
  `0x6c1c`-`0x6dd4`.
- Common tail stores at `0x6dd8`, `0x6de0`, and `0x6de4` remain separate
  32-bit writes despite the decompiler's QWORD tail display.
- The dispatcher call is at `0x7d30`.
- IDA type at `0x6a28` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
