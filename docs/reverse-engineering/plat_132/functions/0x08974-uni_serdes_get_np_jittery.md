# 0x08974 uni_serdes_get_np_jittery

## Status

- Status: complete
- Confidence: verified volatile read, three-bit extraction, vendor log,
  returned value, absent internal xrefs, and exported ABI.
- Size: `0x3c` bytes, 14 ARM64 instructions.
- Recovered signature: `uint32_t uni_serdes_get_np_jittery(void)`.

## Semantics

Reads `uni_serdes_base + 0x48`, extracts bits 6-8, logs the value, and returns
the unsigned three-bit result. It has no internal IDB xrefs and is exported
through `__ksymtab_uni_serdes_get_np_jittery`.

## Evidence

- Complete ARM64 body at `0x8974` through `0x89ac`.
- Volatile load at `0x8988` and `UBFX #6,#3` extraction at `0x8994`.
- Value log at `0x899c`; returned field at `0x89a0`.
- IDA type at `0x8974` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
