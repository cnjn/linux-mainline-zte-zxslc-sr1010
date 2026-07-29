# 0x06df0 mode_eth_2p5gbase_r_cfg

## Status

- Status: complete
- Confidence: verified CPU-132 profile, shared CPU-133/129 profile and probe
  order, all ordered 32-bit stores, common tail, caller context, and void ABI.
- Size: `0x3d8` bytes, 202 ARM64 instructions.
- Recovered signature: `void mode_eth_2p5gbase_r_cfg(void)`.

## Semantics

Logs `mode_eth_2p5gbase_r_cfg`, then selects an Ethernet 2.5GBASE-R SerDes
profile. CPU 132 has priority. If it does not match, the code checks CPU 133;
only if that predicate does not return one does it test CPU 129. CPU 133 and
CPU 129 share the same second profile.

| CPU predicate | Profile writes | Shared tail |
| --- | --- | --- |
| `isCpuType_132() == 1` | 46 ordered 32-bit stores at `0x00..0xb4` | `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0` |
| otherwise, 133 or 129 | 46 different ordered 32-bit stores at `0x00..0xb4` | same three stores |
| neither | no MMIO writes | none |

The dispatcher arguments are unused. The function performs direct ordered
32-bit stores only, with no SerDes read or RMW.

## Return Semantics

The supported paths leave a base-pointer residual; the unsupported path leaves
the CPU-129 predicate result. These are not semantic outputs, so the recovered
ABI is `void`.

## Caller Context

`serdes_mode_set @ 0x7cc0` is the sole direct caller. It invokes this entry for
case 10, which `check_serdes_config @ 0x2a58` labels
`MODE_ETH_USXGMII_2P5G`.

## Evidence

- Complete ARM64 body at `0x6df0` through `0x71c4`.
- CPU-132 gate at `0x6e04`-`0x6e0c`; CPU-133 gate at `0x6fc0`-`0x6fc8`; CPU-129
  fallback gate at `0x71b4`-`0x71bc`.
- CPU-132 stores at `0x6e20`-`0x6fb8`; shared CPU-133/129 stores at
  `0x6fe4`-`0x719c`.
- Common tail stores at `0x71a0`, `0x71a8`, and `0x71ac` are separate 32-bit
  accesses despite the QWORD rendering of the final pair.
- The sole direct call is `serdes_mode_set` case 10 at `0x7d38`.
- IDA type at `0x6df0` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
