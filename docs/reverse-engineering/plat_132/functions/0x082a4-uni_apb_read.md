# 0x082a4 uni_apb_read

## Status

- Status: complete
- Confidence: verified the single 32-bit load, zero-extended return, absent
  internal xrefs, exported symbol context, and pointer ABI.
- Size: `0x8` bytes, 2 ARM64 instructions.
- Recovered signature:
  `uint32_t uni_apb_read(const volatile uint32_t *address)`.

## Semantics

Reads one volatile 32-bit value from `address` and returns it. `LDR W0` clears
the upper half of the architectural return register, so the semantic result is
an unsigned 32-bit value. The function has no internal IDB xrefs and is exported
through `__ksymtab_uni_apb_read` for external users.

## Evidence

- Complete body at `0x82a4`: `LDR W0, [X0]`; `0x82a8`: `RET`.
- No internal code or data xrefs in the current IDB.
- Vendor kallsyms lists exported `T` symbol `uni_apb_read [plat_132]` and
  `__ksymtab_uni_apb_read`.
- IDA type at `0x82a4` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
