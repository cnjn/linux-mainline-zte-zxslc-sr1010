# 0x085e8 uni_serdes_get_band

## Status

- Status: complete
- Confidence: verified volatile read address, eight-bit extraction, vendor log,
  returned value, absent internal xrefs, and exported ABI.
- Size: `0x3c` bytes, 14 ARM64 instructions.
- Recovered signature: `uint32_t uni_serdes_get_band(void)`.

## Semantics

Reads `uni_serdes_base + 0xd0`, extracts bits 16-23, logs the unsigned byte,
and returns it. This reader uses `+0xd0`, distinct from the `+0x6c` register
written by `uni_serdes_set_band`; the source retains the observed addresses
without assuming a bidirectional setter/getter pair. The entry is exported and
has no internal IDB xrefs.

## Evidence

- Complete ARM64 body at `0x85e8` through `0x8620`.
- Volatile load from `+0xd0` at `0x85fc` and `UBFX #16,#8` at `0x8608`.
- Value log at `0x8610`; returned byte at `0x8614`.
- Vendor kallsyms lists exported `T` symbol and `__ksymtab_uni_serdes_get_band`.
- IDA type at `0x85e8` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
