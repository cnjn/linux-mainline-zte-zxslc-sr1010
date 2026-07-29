# 0x08744 uni_serdes_set_error_time_en

## Status

- Status: complete
- Confidence: verified PON-SerDes target block, RMW mask, unmasked shifted
  input, PRBS counter callers, final log return, and exported ABI.
- Size: `0x34` bytes, 12 ARM64 instructions.
- Recovered signature: `int uni_serdes_set_error_time_en(uint32_t value)`.

## Semantics

Despite its `uni_serdes` name, reads `pon_serdes_base + 0x94`, clears bit 30,
ORs `value << 30`, writes the result, logs success, and returns the `printk`
result. The input is not masked before shifting. It is used by
`uni_serdes_get_hard_prbs_cnt @ 0x97dc` and
`uni_serdes_get_prbs_counters @ 0x9880`, and is exported through
`__ksymtab_uni_serdes_set_error_time_en`.

## Evidence

- Complete ARM64 body at `0x8744` through `0x8774`.
- Target pointer load at `0x8750` references `pon_serdes_base`.
- RMW at `0x8754`-`0x8760`: clear mask `0xbfffffff`, raw `W0,LSL#30` OR.
- Final log call at `0x876c`; its result is returned.
- Direct internal calls at `0x9828` and `0x98c0`.
- IDA type at `0x8744` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
