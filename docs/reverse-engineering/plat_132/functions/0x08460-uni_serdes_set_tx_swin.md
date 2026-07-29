# 0x08460 uni_serdes_set_tx_swin

## Status

- Status: complete
- Confidence: verified register RMW, unmasked shifted input, final log return,
  no internal xrefs, and exported ABI.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int uni_serdes_set_tx_swin(uint32_t value)`.

## Semantics

Reads `uni_serdes_base + 0x20`, clears bits 16-17, ORs `value << 16`, writes
the result, logs the raw value, and returns the `printk` result. The input is
not masked before shifting, so values wider than the nominal two-bit field can
affect higher bits. It has no internal IDB xrefs and is exported through
`__ksymtab_uni_serdes_set_tx_swin`.

## Evidence

- Complete ARM64 body at `0x8460` through `0x8494`.
- RMW at `0x8470`-`0x847c`: clear mask `0xfffcffff`, raw `W0,LSL#16` OR.
- Final log call at `0x848c`; its result is returned.
- IDA type at `0x8460` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
