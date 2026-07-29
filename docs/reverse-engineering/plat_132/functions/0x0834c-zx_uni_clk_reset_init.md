# 0x0834c zx_uni_clk_reset_init

## Status

- Status: complete
- Confidence: verified the constant-zero stub, absent internal xrefs, exported
  symbol context, and `int(void)` ABI.
- Size: `0x8` bytes, 2 ARM64 instructions.
- Recovered signature: `int zx_uni_clk_reset_init(void)`.

## Semantics

Returns zero without accessing memory or calling another function. It has no
internal IDB xrefs and is exported through `__ksymtab_zx_uni_clk_reset_init`, so
the successful `int` API is retained for external callers.

## Evidence

- Complete body at `0x834c`: `MOV W0, #0`; `0x8350`: `RET`.
- No internal code or data xrefs in the current IDB.
- Vendor kallsyms lists exported `T` symbol `zx_uni_clk_reset_init [plat_132]`.
- IDA type at `0x834c` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
