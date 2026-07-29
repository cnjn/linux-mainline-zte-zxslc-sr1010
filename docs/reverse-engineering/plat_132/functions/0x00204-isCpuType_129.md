# 0x00204 isCpuType_129

## Status

- Status: complete
- Confidence: verified full global read, literal-four comparison, normalized
  return, and direct callers.
- Size: `0x14` bytes, 5 ARM64 instructions.
- Recovered signature: `u32 isCpuType_129(void)`.

## Semantics

Returns one exactly when `g_pon_cputype == 4`; otherwise returns zero.

## Caller Context

The predicate has 53 direct call sites across PON clock/reset, SerDes, IDM, and
XMAC/PCS configuration paths.

## Evidence

- Complete ARM64 body at `0x204` through `0x214`.
- Exact global load, literal-four compare, and `CSET EQ` normalization.
- Exhaustive direct xref query found 53 code references.
- IDA type at `0x204` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
