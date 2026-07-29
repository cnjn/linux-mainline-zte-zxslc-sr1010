# 0x071c8 mode_eth_2p5gbase_x_cfg

## Status

- Status: complete
- Confidence: verified all three CPU profiles, priority order, 49 ordered
  32-bit writes per supported path, shared tail, caller context, and void ABI.
- Size: `0x560` bytes, 287 ARM64 instructions.
- Recovered signature: `void mode_eth_2p5gbase_x_cfg(void)`.

## Semantics

Logs `mode_eth_2p5gbase_x_cfg`, then selects an Ethernet 2.5GBASE-X SerDes
profile in priority order: CPU 132, CPU 133, then CPU 129. Every matching path
writes 49 raw 32-bit words at offsets `0x00..0xc0`; a nonmatching CPU only
logs and performs no MMIO access.

| CPU predicate | Profile shape |
| --- | --- |
| `isCpuType_132() == 1` | 46 profile words at `0x00..0xb4`, then `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0` |
| otherwise, `isCpuType_133() == 1` | distinct 46-word profile and the same tail |
| otherwise, `isCpuType_129() == 1` | distinct 46-word profile and the same tail |

The dispatcher arguments are unused. The profile consists solely of ordered
32-bit stores; none of its SerDes accesses are reads or RMWs.

## Return Semantics

The log result is discarded. Supported paths retain a base-pointer residual,
and the unsupported path retains the CPU-129 predicate result. These values are
not a semantic output, so the recovered ABI is `void`.

## Caller Context

`serdes_mode_set @ 0x7cc0` is the sole direct caller. Cases 9 and 15 share this
entry; `check_serdes_config @ 0x2a58` labels them `MODE_ETH_HSGMII` and
`MODE_ETH_2P5BASE_X` respectively.

## Evidence

- Complete ARM64 body at `0x71c8` through `0x7724`.
- CPU-132 gate at `0x71dc`-`0x71e4`, CPU-133 gate at `0x738c`-`0x7394`, and
  CPU-129 fallback at `0x7564`-`0x756c`.
- CPU-132 profile stores at `0x71f8`-`0x7384`, CPU-133 stores at
  `0x73b0`-`0x7554` plus its tail values at `0x7700`-`0x770c`, and CPU-129
  stores at `0x7588`-`0x76f8` plus its tail values at `0x7700`-`0x770c`.
- Common tail stores at `0x7710`, `0x7718`, and `0x771c` are individual 32-bit
  accesses despite Hex-Rays displaying the last pair as a QWORD store.
- `serdes_mode_set` calls the shared entry at `0x7d40`.
- IDA type at `0x71c8` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
