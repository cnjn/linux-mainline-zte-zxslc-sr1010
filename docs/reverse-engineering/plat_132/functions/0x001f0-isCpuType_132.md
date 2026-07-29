# 0x001f0 isCpuType_132

## Status

- Status: complete
- Confidence: verified full global read, literal-one comparison, normalized
  return, and direct callers.
- Size: `0x14` bytes, 5 ARM64 instructions.
- Recovered signature: `u32 isCpuType_132(void)`.

## Semantics

Returns one exactly when `g_pon_cputype == 1`; otherwise returns zero.

## Caller Context

The predicate has 30 direct call sites across PON mode/PLL, SerDes, IDM, and
XMAC configuration paths.

## Evidence

- Complete ARM64 body at `0x1f0` through `0x200`.
- Exact global load, literal-one compare, and `CSET EQ` normalization.
- Exhaustive direct xref query found 30 code references.
- IDA type at `0x1f0` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
