# 0x04cc4 mode_10g_epon_nsyn_fifo_cfg

## Status

- Status: complete
- Confidence: verified both full raw profiles, all ordered 32-bit stores,
  CPU-selection order, common tail, ignored dispatcher arguments, caller
  context, and semantic void return.
- Size: `0x3bc` bytes, 198 ARM64 instructions.
- Recovered signature: `void mode_10g_epon_nsyn_fifo_cfg(void)`.

## Semantics

Logs `mode_10g_epon_nsyn_fifo_cfg`, then configures the non-synchronous 10G
EPON FIFO SerDes mode. It follows the same selection geometry as the preceding
EPON scripts:

| CPU predicate | Profile writes | Shared tail |
| --- | --- | --- |
| `isCpuType_132() == 1` | 46 ordered 32-bit stores at `0x00..0xb4` | `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0` |
| otherwise, `isCpuType_133() == 1` | 46 different ordered 32-bit stores at `0x00..0xb4` | same three stores |
| neither | no MMIO writes | none |

The generic arguments passed from `serdes_mode_set` are not read by the body;
it overwrites those argument registers with constants before any use. The
recovered semantic signature is consequently parameterless. All writes are
raw profile constants and no SerDes register is read or RMWed.

## Return Semantics

The vendor log result is discarded. Supported branches leave a base-pointer
residual and unsupported branches leave the CPU-133 predicate result, so this
has no meaningful result value and is recovered as `void`.

## Caller Context

`serdes_mode_set @ 0x7cc0` is the sole direct caller. It selects this function
for switch case 2, `MODE_10G_EPON_NSYN_FIFO`.

## Evidence

- Complete ARM64 body at `0x4cc4` through `0x507c`.
- CPU-132 gate at `0x4cd8`-`0x4ce0`; CPU-133 fallback begins at `0x4e88`.
- CPU-132 profile stores `0x4cfc`-`0x4e80`; CPU-133 profile stores
  `0x4eac`-`0x5064`.
- Common tail writes at `0x5068`, `0x5070`, and `0x5074` are individual 32-bit
  stores, despite Hex-Rays combining the last two in pseudocode.
- The only direct call is `serdes_mode_set` case 2 at `0x7cf8`.
- IDA type at `0x4cc4` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
