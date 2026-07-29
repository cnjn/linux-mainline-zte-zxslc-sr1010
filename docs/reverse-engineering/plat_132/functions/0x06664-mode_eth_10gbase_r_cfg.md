# 0x06664 mode_eth_10gbase_r_cfg

## Status

- Status: complete
- Confidence: verified both CPU profiles, all ordered 32-bit stores, common
  tail, dual dispatcher-case context, and semantic void ABI.
- Size: `0x3c4` bytes, 196 ARM64 instructions.
- Recovered signature: `void mode_eth_10gbase_r_cfg(void)`.

## Semantics

Logs `mode_eth_10gbase_r_cfg`, then selects an Ethernet 10GBASE-R SerDes
profile. CPU 132 has priority; CPU 133 is considered only when the 132 predicate
does not return exactly one.

| CPU predicate | Profile writes | Shared tail |
| --- | --- | --- |
| `isCpuType_132() == 1` | 46 ordered 32-bit stores at `0x00..0xb4` | `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0` |
| otherwise, `isCpuType_133() == 1` | 46 different ordered 32-bit stores at `0x00..0xb4` | same three stores |
| neither | no MMIO writes | none |

The dispatcher supplies generic arguments that this body never consumes. All
accesses are direct ordered 32-bit stores; no SerDes register is read or RMWed.

## Return Semantics

The initial log result is discarded. Supported paths retain a base-pointer
residual, while the unsupported path retains the CPU-133 predicate result.
These are not semantic results, so the recovered ABI is `void`.

## Caller Context

`serdes_mode_set @ 0x7cc0` is the sole direct caller. Its switch cases 13
(`MODE_ETH_10GBASE_R`) and 14 (`MODE_ETH_USXGMII_10G`) share this entry.

## Evidence

- Complete ARM64 body at `0x6664` through `0x6a24`.
- CPU-132 gate at `0x6678`-`0x6680`; CPU-133 fallback at
  `0x6830`-`0x6838`.
- CPU-132 stores at `0x669c`-`0x6828`; CPU-133 stores at
  `0x6854`-`0x6a0c`.
- Common tail stores at `0x6a10`, `0x6a18`, and `0x6a1c` are separate 32-bit
  accesses despite Hex-Rays presenting the last two as a QWORD store.
- `serdes_mode_set` calls the shared entry at `0x7d28`.
- IDA type at `0x6664` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
