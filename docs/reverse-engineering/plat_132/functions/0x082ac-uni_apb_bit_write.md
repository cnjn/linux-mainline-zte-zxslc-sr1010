# 0x082ac uni_apb_bit_write

## Status

- Status: complete
- Confidence: verified field-mask construction, one volatile read/RMW/write,
  unmasked input-value behavior, pointer return, and exported ABI.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature:
  `volatile uint32_t *uni_apb_bit_write(volatile uint32_t *address, uint32_t value, uint32_t width, uint32_t shift)`.

## Semantics

Builds `((1U << width) - 1U) << shift`, clears that field in the volatile word
at `address`, ORs `value << shift`, writes the result back, and returns the
unchanged address. The input value is not masked before the OR, matching the
binary exactly. The helper has no internal IDB xrefs and is exported through
`__ksymtab_uni_apb_bit_write` for external callers.

## Evidence

- Complete body at `0x82ac` through `0x82d0`.
- Volatile load at `0x82b0`, width and shift construction at `0x82b4`-`0x82c0`,
  BIC/ORR RMW at `0x82c4`-`0x82c8`, and store at `0x82cc`.
- No internal code or data xrefs in the current IDB.
- Vendor kallsyms lists exported `T` symbol and
  `__ksymtab_uni_apb_bit_write`.
- IDA type at `0x82ac` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
