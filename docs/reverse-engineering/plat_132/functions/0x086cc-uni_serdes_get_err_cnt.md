# 0x086cc uni_serdes_get_err_cnt

## Status

- Status: complete
- Confidence: verified both volatile counter loads, 16-bit high-word packing,
  64-bit return/log behavior, callers, and exported ABI.
- Size: `0x48` bytes, 18 ARM64 instructions.
- Recovered signature: `uint64_t uni_serdes_get_err_cnt(void)`.

## Semantics

Reads a 48-bit Uni SerDes error count: the full 32-bit low word from
`uni_serdes_base + 0xe8` and the low 16 bits of `+0xec` shifted into bits 32-47.
It logs and returns the packed unsigned 64-bit value. Bits 16-31 of the second
register are discarded.

## Caller Context

Called by `uni_serdesPrbsCounterGetHandler @ 0x87fc` at `0x8804`,
`uni_serdes_get_hard_prbs_cnt @ 0x97dc` at `0x984c`, and
`uni_serdes_get_prbs_counters @ 0x9880` at `0x9924`. It is also exported through
`__ksymtab_uni_serdes_get_err_cnt`.

## Evidence

- Complete ARM64 body at `0x86cc` through `0x8710`.
- Low-word load at `0x86e0`, high-word load at `0x86e4`, and `UBFIZ #32,#16`
  packing at `0x86ec`.
- Packed OR at `0x86f0`, log at `0x8700`, and 64-bit return at `0x8704`.
- IDA type at `0x86cc` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
