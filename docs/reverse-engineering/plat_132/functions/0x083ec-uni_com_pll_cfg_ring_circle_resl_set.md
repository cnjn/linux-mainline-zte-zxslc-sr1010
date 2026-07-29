# 0x083ec uni_com_pll_cfg_ring_circle_resl_set

## Status

- Status: complete
- Confidence: verified register RMW, unmasked shifted input, final log return,
  export metadata context, and `int` ABI.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int uni_com_pll_cfg_ring_circle_resl_set(uint32_t value)`.

## Semantics

Reads `uni_serdes_base + 0x04`, clears bits 23-26, ORs `value << 23`, writes
the result, logs the raw value, and returns the `printk` result. The input is
not masked before shifting, so higher input bits may affect bits above the
nominal four-bit field. The sole current xref is its export metadata.

## Evidence

- Complete ARM64 body at `0x83ec` through `0x8420`.
- RMW at `0x83fc`-`0x8408`: clear mask `0xf87fffff`, raw `W0,LSL#23` OR.
- Final log call at `0x8418`; its result is returned.
- Vendor kallsyms lists exported `T` symbol and
  `__ksymtab_uni_com_pll_cfg_ring_circle_resl_set`.
- IDA type at `0x83ec` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
