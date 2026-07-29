# 0x08840 uni_serdes_set_rx_eq_mbf

## Status

- Status: complete
- Confidence: verified register RMW, unmasked shifted input, final log return,
  absent internal xrefs, and exported ABI.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int uni_serdes_set_rx_eq_mbf(uint32_t value)`.

## Semantics

Reads `uni_serdes_base + 0x2c`, clears bits 18-21, ORs `value << 18`, writes
the result, logs the raw value, and returns the `printk` result. The input is
not masked before shifting, so bits beyond the nominal four-bit MBF field may
affect higher bits. The entry is exported through
`__ksymtab_uni_serdes_set_rx_eq_mbf` and has no internal IDB xrefs.

## Evidence

- Complete ARM64 body at `0x8840` through `0x8874`.
- RMW at `0x8850`-`0x885c`: clear mask `0xffc3ffff`, raw `W0,LSL#18` OR.
- Final log call at `0x886c`; its result is returned.
- IDA type at `0x8840` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
