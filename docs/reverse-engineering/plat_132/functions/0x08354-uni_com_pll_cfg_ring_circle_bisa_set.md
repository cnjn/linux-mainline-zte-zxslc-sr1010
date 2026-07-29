# 0x08354 uni_com_pll_cfg_ring_circle_bisa_set

## Status

- Status: complete
- Confidence: verified register RMW, unmasked shifted input, both vendor logs,
  final log return, absent internal xrefs, and exported `int` ABI.
- Size: `0x50` bytes, 19 ARM64 instructions.
- Recovered signature: `int uni_com_pll_cfg_ring_circle_bisa_set(uint32_t value)`.

## Semantics

Reads `uni_serdes_base + 0x04`, clears bits 16-19, ORs `value << 16`, and
writes the result back. The input is not masked before shifting, so bits above
the nominal low four-bit field can affect higher register bits; this is retained
exactly. It logs the vendor ring-circle mapping string, then returns the result
of logging `value` in hexadecimal.

The entry has no internal IDB xrefs and is exported through
`__ksymtab_uni_com_pll_cfg_ring_circle_bisa_set`.

## Evidence

- Complete ARM64 body at `0x8354` through `0x83a0`.
- RMW at `0x836c`-`0x8378`: clear mask `0xfff0ffff`, followed by raw
  `W19,LSL#16` OR.
- Vendor log calls at `0x8384` and `0x8394`; the latter result is returned.
- No internal code or data xrefs in the current IDB.
- IDA type at `0x8354` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
