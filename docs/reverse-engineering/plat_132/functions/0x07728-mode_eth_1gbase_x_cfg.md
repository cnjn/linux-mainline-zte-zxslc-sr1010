# 0x07728 mode_eth_1gbase_x_cfg

## Status

- Status: complete
- Confidence: verified all three CPU profiles, priority order, 49 ordered
  32-bit writes per supported path, shared tail, caller context, and void ABI.
- Size: `0x54c` bytes, 284 ARM64 instructions.
- Recovered signature: `void mode_eth_1gbase_x_cfg(void)`.

## Semantics

Logs `mode_eth_1gbase_x_cfg`, then selects an Ethernet 1GBASE-X SerDes profile
in priority order: CPU 132, CPU 133, then CPU 129. Each supported path writes
49 raw 32-bit values at `pon_serdes_base + 0x00..0xc0`; unsupported CPUs only
log and leave the block untouched.

| CPU predicate | Profile shape |
| --- | --- |
| `isCpuType_132() == 1` | 46 profile words at `0x00..0xb4`, then `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0` |
| otherwise, `isCpuType_133() == 1` | distinct 46-word profile and the same tail |
| otherwise, `isCpuType_129() == 1` | distinct 46-word profile and the same tail |

The dispatcher's generic arguments are ignored. All MMIO effects are direct,
ordered 32-bit stores; no register is read or modified in place.

## Return Semantics

The log result is discarded. Supported paths retain a base-pointer residual and
the unsupported path retains the CPU-129 predicate result. The semantic ABI is
therefore `void`.

## Caller Context

`serdes_mode_set @ 0x7cc0` is the sole direct caller. Cases 8 and 16 share this
entry; `check_serdes_config @ 0x2a58` labels them `MODE_ETH_SGMII` and
`MODE_ETH_1GBASE_X` respectively.

## Evidence

- Complete ARM64 body at `0x7728` through `0x7c70`.
- CPU-132 gate at `0x773c`-`0x7744`, CPU-133 gate at `0x78ec`-`0x78f4`, and
  CPU-129 fallback at `0x7ab4`-`0x7abc`.
- CPU-132 stores at `0x7760`-`0x78e4`, CPU-133 stores at
  `0x790c`-`0x7aa4` plus tail values at `0x7c4c`-`0x7c58`, and CPU-129 stores
  at `0x7ad8`-`0x7c44` plus the same tail instructions.
- Common tail writes at `0x7c5c`, `0x7c64`, and `0x7c68` are individual 32-bit
  stores despite Hex-Rays displaying the final pair as a QWORD store.
- `serdes_mode_set` calls this shared entry at `0x7d48`.
- IDA type at `0x7728` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
