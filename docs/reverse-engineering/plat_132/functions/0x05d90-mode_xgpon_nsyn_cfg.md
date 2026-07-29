# 0x05d90 mode_xgpon_nsyn_cfg

## Status

- Status: complete
- Confidence: verified both CPU paths, the two nested CPU-133 gates in the
  CPU-132 path, all ordered 32-bit stores, shared tail, caller context, and
  semantic void ABI.
- Size: `0x3e4` bytes, 208 ARM64 instructions.
- Recovered signature: `void mode_xgpon_nsyn_cfg(void)`.

## Semantics

Logs `mode_xgpon_nsyn_cfg`, then applies the Mode-6 non-synchronous XGPON
SerDes profile. CPU 132 has priority; CPU 133 is considered only if the 132
predicate did not return exactly one.

| CPU predicate | Profile writes | Shared tail |
| --- | --- | --- |
| `isCpuType_132() == 1` | 44 ordered 32-bit stores at `0x00..0xb4`, excluding `0x20` and `0x2c`; each omitted location has its own intervening `isCpuType_133() == 1` gate | `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0` |
| otherwise, `isCpuType_133() == 1` | 46 ordered 32-bit stores at `0x00..0xb4` | same three stores |
| neither | no MMIO writes | none |

The helpers compare one shared CPU-type global against distinct values, so the
two nested CPU-133 predicates are false while an unchanged CPU-132 predicate
is true. Their conditional writes, `0x20=0x8f000000` and `0x2c=0xaa8`, are
nevertheless retained exactly because they are present in the binary.

The dispatcher passes two generic mode arguments, but the body consumes neither
before replacing the argument registers with constants. All MMIO stores are
ordered 32-bit accesses; no SerDes register is read or RMWed.

## Return Semantics

The log result is discarded. Supported paths retain a base-pointer residual;
the unsupported path retains the CPU-133 predicate result. Neither is a
semantic result, so the recovered ABI is `void`.

## Caller Context

`serdes_mode_set @ 0x7cc0` is the sole direct caller, invoking this profile for
case 6. `check_serdes_config @ 0x2a58` labels mode 6 `MODE_XGPON_NSYN`.

## Evidence

- Complete ARM64 body at `0x5d90` through `0x6170`.
- CPU-132 gate at `0x5da8`-`0x5db0`; nested CPU-133 gates at
  `0x5e20`-`0x5e28` and `0x5e54`-`0x5e5c`; CPU-133 fallback at
  `0x5f7c`-`0x5f84`.
- CPU-132 stores at `0x5dc4`-`0x5f74`; CPU-133 stores at
  `0x5fa0`-`0x6154`.
- Common tail stores at `0x6158`, `0x6160`, and `0x6164` are separate 32-bit
  writes despite Hex-Rays displaying the final two as a QWORD store.
- The only direct call is `serdes_mode_set` case 6 at `0x7d18`.
- IDA type at `0x5d90` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
