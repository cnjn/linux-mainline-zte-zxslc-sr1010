# 0x0543c mode_10g_epon_syn_cfg

## Status

- Status: complete
- Confidence: verified CPU-133-only gate, all 49 ordered 32-bit profile
  stores, ignored dispatcher arguments, caller context, and residual return.
- Size: `0x208` bytes, 105 ARM64 instructions.
- Recovered signature: `void mode_10g_epon_syn_cfg(void)`.

## Semantics

Logs `mode_10g_epon_syn_cfg`, then checks `isCpuType_133() == 1`. A nonmatch
does no MMIO access. A match writes 49 raw 32-bit values in order at
`pon_serdes_base + 0x00 .. 0xc0`, inclusive. The final three writes are
`0xb8=0x80`, `0xbc=0x10000`, and `0xc0=0`; the binary emits three separate
32-bit stores even though Hex-Rays merges the last two in pseudocode.

`serdes_mode_set` supplies generic arguments at its case-4 call site, but this
body never consumes them. The profile constants are raw hardware values and no
SerDes register is read or RMWed.

## Return Semantics

On CPU 133, a residual base pointer remains in `w0`; otherwise the CPU predicate
result remains. The initial log result is discarded. The recovered semantic ABI
is therefore `void`.

## Caller Context

`serdes_mode_set @ 0x7cc0` is the sole direct caller, selecting this profile
for case 4, `MODE_10G_EPON_SYN`.

## Evidence

- Complete ARM64 body at `0x543c` through `0x5640`.
- CPU-133 predicate gate at `0x5450`-`0x5458`.
- Profile stores at `0x5474` through `0x5638`; all are `STR W` instructions.
- The sole direct call is `serdes_mode_set` case 4 at `0x7d08`.
- IDA type at `0x543c` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
