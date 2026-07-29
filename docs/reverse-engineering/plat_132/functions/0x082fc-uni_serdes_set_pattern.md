# 0x082fc uni_serdes_set_pattern

## Status

- Status: complete
- Confidence: verified parameter widths, ordered register transaction,
  exact-enable predicate, final-control return, and exported ABI.
- Size: `0x50` bytes, 20 ARM64 instructions.
- Recovered signature:
  `uint32_t uni_serdes_set_pattern(uint32_t pattern_low, uint32_t pattern_high, uint16_t control_low, int enable)`.

## Semantics

Performs an ordered Uni SerDes pattern transaction:

1. Clears bits 12-15 at `uni_serdes_base + 0x94`.
2. Stores the two 32-bit pattern arguments at `+0x9c` and `+0xa0`.
3. Replaces only the low 16 bits of `+0xa4` with the zero-extended third
   argument, preserving its high half.
4. If `enable == 1`, sets `+0xa4` bits 16-18; any other value clears them.
5. Stores and returns the final `+0xa4` value.

There are no internal IDB xrefs. The entry is exported through
`__ksymtab_uni_serdes_set_pattern`.

## Evidence

- Complete ARM64 body at `0x82fc` through `0x8348`.
- `UXTH W2` at `0x8300` establishes the 16-bit third argument.
- `+0x94` clear at `0x830c`-`0x8314`, pattern stores at `0x8318` and `0x831c`,
  and low-half replacement at `0x8320`-`0x832c`.
- Exact `enable == 1` branch at `0x8304`/`0x8334`; set/clear paths at
  `0x8338` and `0x8340`.
- IDA type at `0x82fc` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
