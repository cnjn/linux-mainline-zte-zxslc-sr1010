# 0x08714 uni_serdes_prbs_err_ok

## Status

- Status: complete
- Confidence: verified one ordered RMW, final log return, absent internal
  xrefs, and exported ABI.
- Size: `0x30` bytes, 11 ARM64 instructions.
- Recovered signature: `int uni_serdes_prbs_err_ok(void)`.

## Semantics

Reads `uni_serdes_base + 0x48`, sets bit 23, writes the result back, logs
`set 1 bit error ok`, and returns the `printk` result. It has no internal IDB
xrefs and is exported through `__ksymtab_uni_serdes_prbs_err_ok`.

## Evidence

- Complete ARM64 body at `0x8714` through `0x8740`.
- RMW at `0x8724`-`0x872c`: OR mask `0x00800000`.
- Final log call at `0x8738`; its result is returned.
- IDA type at `0x8714` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
