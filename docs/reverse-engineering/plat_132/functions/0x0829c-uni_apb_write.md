# 0x0829c uni_apb_write

## Status

- Status: complete
- Confidence: verified the single 32-bit store, untouched pointer return,
  absent internal xrefs, exported symbol context, and pointer/value ABI.
- Size: `0x8` bytes, 2 ARM64 instructions.
- Recovered signature:
  `volatile uint32_t *uni_apb_write(volatile uint32_t *address, uint32_t value)`.

## Semantics

Stores `value` as one volatile 32-bit access at `address`, then returns the
unchanged address. The entry has no internal code or data xrefs in the current
IDB and is exported through `__ksymtab_uni_apb_write`, so the pointer-returning
ABI is retained for external callers.

## Evidence

- Complete body at `0x829c`: `STR W1, [X0]`; `0x82a0`: `RET`.
- No internal xrefs in the current IDB.
- Vendor kallsyms lists exported `T` symbol `uni_apb_write [plat_132]` and
  `__ksymtab_uni_apb_write`.
- IDA type at `0x829c` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
