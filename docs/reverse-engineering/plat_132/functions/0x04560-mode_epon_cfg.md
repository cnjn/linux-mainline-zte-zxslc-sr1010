# 0x04560 mode_epon_cfg

## Status

- Status: complete
- Confidence: verified both complete raw register profiles, every 32-bit store
  width/order, CPU-selection order, common tail writes, caller context, and
  residual-return behavior.
- Size: `0x3a8` bytes, 197 ARM64 instructions.
- Recovered signature: `void mode_epon_cfg(void)`.

## Semantics

Logs `mode_epon_cfg`, then selects one of two complete PON SerDes profiles:

| CPU predicate | Profile writes | Shared tail |
| --- | --- | --- |
| `isCpuType_132() == 1` | 46 32-bit writes at offsets `0x00..0xb4` | `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0` |
| otherwise, `isCpuType_133() == 1` | 46 different 32-bit writes at offsets `0x00..0xb4` | same three writes |
| neither | no MMIO writes | none |

CPU 132 has priority: CPU 133 is queried only when CPU 132 did not return
exactly 1. All profile values are hardware-specific and deliberately retained
as raw constants in the source reconstruction. The script contains no reads or
RMWs, only ordered 32-bit stores.

The profiles differ at many offsets, including `0x00`, `0x04`, `0x08`,
`0x40`, `0x60`, `0x64`, `0x6c`-`0x88`, and `0x94`-`0xb4`. The three common tail
writes occur only after either complete profile, not after the unsupported-CPU
path.

## Return Semantics

The initial `printk` result is discarded. Supported profiles leave a residual
base pointer in `w0`; unsupported CPUs leave the `isCpuType_133()` result.
These values do not form a useful result contract, so the recovered semantic
ABI is `void`.

## Caller Context

`serdes_mode_set @ 0x7cc0` is the sole direct caller. Its switch invokes this
function for mode 0, the EPON mode. No other direct code or data xrefs target
the entry.

## Evidence

- Complete ARM64 body at `0x4560` through `0x4904`.
- Log and CPU-132 gate at `0x4564`-`0x457c`; CPU-133 fallback gate at
  `0x4720`-`0x4728`.
- CPU-132 profile stores `0x4590`-`0x4718`; CPU-133 profile stores
  `0x4740`-`0x48ec`.
- Common tail stores at `0x48f0`, `0x48f8`, and `0x48fc` are all 32-bit,
  including the zero at `0xc0`; do not replace them with a wide store.
- `serdes_mode_set` calls `mode_epon_cfg` at `0x7ce8` only for switch case 0.
- IDA type at `0x4560` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
