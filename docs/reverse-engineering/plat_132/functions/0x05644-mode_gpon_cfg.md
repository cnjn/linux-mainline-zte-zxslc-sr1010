# 0x05644 mode_gpon_cfg

## Status

- Status: complete
- Confidence: verified all CPU-129/132/133 profiles, their priority order,
  every ordered 32-bit store, profile-dependent tail words, common tail,
  ignored dispatcher arguments, caller context, and semantic void ABI.
- Size: `0x564` bytes, 287 ARM64 instructions.
- Recovered signature: `void mode_gpon_cfg(void)`.

## Semantics

Logs `mode_gpon_cfg`, then selects exactly one GPON SerDes profile in priority
order: CPU 129, CPU 132, then CPU 133. An unsupported CPU only receives the
log and no MMIO writes.

| CPU predicate | Profile shape | Common tail |
| --- | --- | --- |
| `isCpuType_129() == 1` | 43 stores at `0x00..0xa8`, then `0xac=0x201c`, `0xb0=0x0c`, `0xb4=0x01000000` | `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0` |
| otherwise, `isCpuType_132() == 1` | 46 stores at `0x00..0xb4`, including `0xac=0x0d`, `0xb0=0`, `0xb4=0` | same three stores |
| otherwise, `isCpuType_133() == 1` | 46 stores at `0x00..0xb4`, including `0xac=0x40002000`, `0xb0=0x0c`, `0xb4=0x01000000` | same three stores |

The binary shares instructions for the CPU-129 and CPU-133 words at offsets
`0xac`, `0xb0`, and `0xb4`, but their incoming register values differ. The
reconstruction writes their resulting values explicitly to make that behavior
auditable. All MMIO accesses are ordered 32-bit stores; none are reads or RMWs.

The dispatcher arguments at the call site are ignored by the body.

## Return Semantics

Supported paths leave a residual base pointer in `w0`; unsupported paths retain
the CPU-133 predicate result. The initial log result is discarded. The
recovered ABI is `void`.

## Caller Context

`serdes_mode_set @ 0x7cc0` is the sole direct caller. It invokes this function
for case 5, `MODE_GPON`.

## Evidence

- Complete ARM64 body at `0x5644` through `0x5ba4`.
- CPU selection gates: 129 at `0x5658`-`0x5660`, 132 at `0x5808`-`0x5810`, and
  133 at `0x59b4`-`0x59bc`.
- Profile store ranges: 129 `0x567c`-`0x57fc`, 132 `0x5824`-`0x59ac`, and 133
  `0x59d8`-`0x5b8c`.
- Common tail stores at `0x5b90`, `0x5b98`, and `0x5b9c` are separate 32-bit
  stores, including the zero at `0xc0`.
- The only direct call is `serdes_mode_set` case 5 at `0x7d10`.
- IDA type at `0x5644` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
