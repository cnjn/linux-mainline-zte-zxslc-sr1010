# 0x082d4 uni_serdes_err_cnt_reset

## Status

- Status: complete
- Confidence: verified both ordered RMWs, zero return, PRBS-counter callers,
  exported symbol context, and `int(void)` ABI.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature: `int uni_serdes_err_cnt_reset(void)`.

## Semantics

Reads `uni_serdes_base + 0x94`, clears bit 15 and writes it back, then rereads
the register, sets bit 15, and writes it back. It returns zero. This explicit
low-to-high pulse resets the Uni SerDes error counter.

## Caller Context

`uni_serdes_get_hard_prbs_cnt @ 0x97dc` and
`uni_serdes_get_prbs_counters @ 0x9880` each call it before reading counter
data. It is also exported through `__ksymtab_uni_serdes_err_cnt_reset`.

## Evidence

- Complete body at `0x82d4` through `0x82f8`.
- First RMW at `0x82dc`-`0x82e4`; second RMW at `0x82e8`-`0x82f0`.
- Constant-zero return at `0x82f4`.
- Direct internal calls at `0x97f4` and `0x9914`.
- IDA type at `0x82d4` set to the recovered semantic signature and Hex-Rays
  cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
