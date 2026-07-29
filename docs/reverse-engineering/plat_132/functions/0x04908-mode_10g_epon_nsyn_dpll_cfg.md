# 0x04908 mode_10g_epon_nsyn_dpll_cfg

## Status

- Status: complete
- Confidence: verified both complete raw profiles, every 32-bit MMIO store,
  CPU-selection order, common tail writes, ignored caller arguments, caller
  context, and semantic void ABI.
- Size: `0x3bc` bytes, 199 ARM64 instructions.
- Recovered signature: `void mode_10g_epon_nsyn_dpll_cfg(void)`.

## Semantics

Logs `mode_10g_epon_nsyn_dpll_cfg`, then applies a non-synchronous 10G EPON
DPLL SerDes profile. CPU 132 has priority; CPU 133 is considered only when the
132 predicate did not return exactly 1.

| CPU predicate | Profile writes | Shared tail |
| --- | --- | --- |
| `isCpuType_132() == 1` | 46 ordered 32-bit stores at `0x00..0xb4` | `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0` |
| otherwise, `isCpuType_133() == 1` | 46 different ordered 32-bit stores at `0x00..0xb4` | same three stores |
| neither | no MMIO writes | none |

The dispatcher passes two generic mode arguments at its call site, but this
body never consumes either input register before overwriting them with profile
constants. The recovered semantic signature therefore has no parameters.

All profile constants remain raw in the source reconstruction. The code only
performs ordered 32-bit stores; it does not read or RMW a SerDes register.

## Return Semantics

The initial log result is discarded. Supported paths retain a base-pointer
residual in `w0`, while unsupported paths retain `isCpuType_133()`'s result.
These values are not a semantic result, so the recovered ABI is `void`.

## Caller Context

`serdes_mode_set @ 0x7cc0` is the sole direct caller. It invokes this profile
for switch case 1, labeled `MODE_10G_EPON_NSYN_DPLL` by
`check_serdes_config`.

## Evidence

- Complete ARM64 body at `0x4908` through `0x4cc0`.
- CPU-132 gate at `0x491c`-`0x4924`; CPU-133 fallback at `0x4ac8`-`0x4ad0`.
- CPU-132 stores at `0x4944`-`0x4ac0`; CPU-133 stores at
  `0x4aec`-`0x4ca8`.
- Common tail stores at `0x4cac`, `0x4cb4`, and `0x4cb8` are separate 32-bit
  accesses and must not be merged into a wide store.
- `serdes_mode_set` calls this function at `0x7cf0` only for case 1.
- IDA type at `0x4908` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
