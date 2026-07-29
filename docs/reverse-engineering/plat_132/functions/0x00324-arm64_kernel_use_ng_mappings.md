# 0x00324 arm64_kernel_use_ng_mappings

## Status

- Status: complete
- Confidence: verified capability-readiness branch, both data sources,
  normalized return, and all direct callers.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `u32 arm64_kernel_use_ng_mappings(void)`.

## Semantics

When `arm64_const_caps_ready > 0`, returns whether `cpu_hwcap_keys[23]` is
positive. Otherwise it returns bit 23 of `cpu_hwcaps`.

## Caller Context

`zx_pon_probe @ 0x580` has three direct call sites.

## Evidence

- Complete ARM64 body at `0x324` through `0x358`.
- Exact readiness threshold, key index 23, fallback bit 23, and normalized
  return behavior.
- Exhaustive direct xref query found three `zx_pon_probe` call sites.
- IDA type at `0x324` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
