# 0x08694 uni_serdes_set_err_cnt_en

## Status

- Status: complete
- Confidence: verified register RMW, unmasked shifted input, two PRBS callers,
  final log return, and exported ABI.
- Size: `0x38` bytes, 13 ARM64 instructions.
- Recovered signature: `int uni_serdes_set_err_cnt_en(uint32_t value)`.

## Semantics

Reads `uni_serdes_base + 0x94`, clears bit 15, ORs `value << 15`, writes the
result, logs the raw value, and returns the `printk` result. The input is not
masked before shifting. `uni_serdes_set_sprbsrxbist @ 0x96a8` calls it at
`0x9714` and `uni_serdes_get_hard_prbs_cnt @ 0x97dc` calls it at `0x9820`.
The entry is exported through `__ksymtab_uni_serdes_set_err_cnt_en`.

## Evidence

- Complete ARM64 body at `0x8694` through `0x86c8`.
- RMW at `0x86a4`-`0x86b0`: clear mask `0xffff7fff`, raw `W0,LSL#15` OR.
- Final log call at `0x86c0`; its result is returned.
- Direct internal calls at `0x9714` and `0x9820`.
- IDA type at `0x8694` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
