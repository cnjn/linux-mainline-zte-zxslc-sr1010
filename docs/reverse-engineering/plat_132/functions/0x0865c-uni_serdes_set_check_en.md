# 0x0865c uni_serdes_set_check_en

## Status

- Status: complete
- Confidence: verified register RMW, unmasked shifted input, two PRBS callers,
  final log return, and exported ABI.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int uni_serdes_set_check_en(uint32_t value)`.

## Semantics

Reads `uni_serdes_base + 0x94`, clears bit 14, ORs `value << 14`, writes the
result, logs the raw value, and returns the `printk` result. The input is not
masked before shifting. `uni_serdes_set_sprbsrxbist @ 0x96a8` calls it at
`0x970c` and `uni_serdes_get_hard_prbs_cnt @ 0x97dc` calls it at `0x9818`.
The entry is exported through `__ksymtab_uni_serdes_set_check_en`.

## Evidence

- Complete ARM64 body at `0x865c` through `0x8690`.
- RMW at `0x866c`-`0x8678`: clear mask `0xffffbfff`, raw `W0,LSL#14` OR.
- Final log call at `0x8688`; its result is returned.
- Direct internal calls at `0x970c` and `0x9818`.
- IDA type at `0x865c` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
