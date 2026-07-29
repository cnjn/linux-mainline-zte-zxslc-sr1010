# 0x085a4 uni_serdes_set_band

## Status

- Status: complete
- Confidence: verified both ordered RMWs, unmasked input behavior, final log
  return, absent internal xrefs, and exported ABI.
- Size: `0x44` bytes, 16 ARM64 instructions.
- Recovered signature: `int uni_serdes_set_band(uint32_t band_select, uint32_t band_value)`.

## Semantics

Performs two RMWs at `uni_serdes_base + 0x6c` in order:

1. Clears bit 14, then ORs unmasked `band_select << 14`.
2. Clears the low byte, then ORs unmasked `band_value`.

Both inputs are raw OR operands rather than field-masked values, so higher bits
can affect register bits outside the nominal fields. It logs `set pll band ok`
and returns the final `printk` result. The helper has no internal IDB xrefs and
is exported through `__ksymtab_uni_serdes_set_band`.

## Evidence

- Complete ARM64 body at `0x85a4` through `0x85e4`.
- First RMW at `0x85b4`-`0x85c0`: clear mask `0xffffbfff`, raw shift by 14.
- Second RMW at `0x85c4`-`0x85d8`: clear mask `0xffffff00`, raw OR.
- Final log call at `0x85dc`; its result is returned.
- IDA type at `0x85a4` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
