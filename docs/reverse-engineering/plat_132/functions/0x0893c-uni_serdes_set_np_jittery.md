# 0x0893c uni_serdes_set_np_jittery

## Status

- Status: complete
- Confidence: verified register RMW, unmasked shifted input, final log return,
  absent internal xrefs, and exported ABI.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int uni_serdes_set_np_jittery(uint32_t value)`.

## Semantics

Reads `uni_serdes_base + 0x48`, clears bits 6-8, ORs `value << 6`, writes the
result, logs the raw value, and returns the `printk` result. The input is not
masked before shifting, so bits beyond the nominal three-bit field may affect
higher bits. The entry is exported through `__ksymtab_uni_serdes_set_np_jittery`
and has no internal IDB xrefs.

## Evidence

- Complete ARM64 body at `0x893c` through `0x8970`.
- RMW at `0x894c`-`0x8958`: clear mask `0xfffffe3f`, raw `W0,LSL#6` OR.
- Final log call at `0x8968`; its result is returned.
- IDA type at `0x893c` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
