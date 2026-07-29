# 0x001dc isCpuType_133

## Status

- Status: complete
- Confidence: verified full global read, literal-two comparison, normalized
  return, and direct callers.
- Size: `0x14` bytes, 5 ARM64 instructions.
- Recovered signature: `u32 isCpuType_133(void)`.

## Semantics

Returns one exactly when `g_pon_cputype == 2`; otherwise returns zero.

## Caller Context

The predicate has 67 direct call sites across PON clock/reset, SerDes, IDM, and
XMAC/PCS configuration paths.

## Evidence

- Complete ARM64 body at `0x1dc` through `0x1ec`.
- Exact global load, literal-two compare, and `CSET EQ` normalization.
- Exhaustive direct xref query found 67 code references.
- IDA type at `0x1dc` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
