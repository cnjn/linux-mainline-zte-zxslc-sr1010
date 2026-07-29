# 0x08624 uni_serdes_set_gen_en

## Status

- Status: complete
- Confidence: verified register RMW, unmasked shifted input, PRBS-mode caller,
  final log return, and exported ABI.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int uni_serdes_set_gen_en(uint32_t value)`.

## Semantics

Reads `uni_serdes_base + 0x94`, clears bit 13, ORs `value << 13`, writes the
result, logs the raw value, and returns the `printk` result. `value` is not
masked before shifting, so input bits above the nominal enable bit can affect
higher bits. `uni_serdes_set_tx_prbs_mode @ 0x941c` calls this entry at
`0x9430`; the function is also exported through `__ksymtab_uni_serdes_set_gen_en`.

## Evidence

- Complete ARM64 body at `0x8624` through `0x8658`.
- RMW at `0x8634`-`0x8640`: clear mask `0xffffdfff`, raw `W0,LSL#13` OR.
- Final log call at `0x8650`; its result is returned.
- Direct internal call at `0x9430` and export-metadata data xref.
- IDA type at `0x8624` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
