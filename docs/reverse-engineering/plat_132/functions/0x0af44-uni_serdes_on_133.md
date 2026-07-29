# 0x0af44 uni_serdes_on_133

## Status

- Status: complete
- Confidence: verified one volatile RMW, returned post-write value, and local
  entry context.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `uint32_t uni_serdes_on_133(void)`.

## Semantics

Reads `uni_serdes_base + 0x54`, sets bit 0, writes the result back, and returns
the written 32-bit value. Despite the name, this body contains no CPU-type
predicate.

## Caller Context

No internal IDB xrefs target this local text (`t`) entry.

## Evidence

- Complete ARM64 body at `0xaf44` through `0xaf58`.
- Volatile load, OR bit-zero, store at `0xaf4c`-`0xaf54`; the unchanged `w0`
  is returned at `0xaf58`.
- IDA type at `0xaf44` set to the recovered unsigned signature and Hex-Rays
  cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
