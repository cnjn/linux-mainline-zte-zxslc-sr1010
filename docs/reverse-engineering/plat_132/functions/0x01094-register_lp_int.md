# 0x01094 register_lp_int

## Status

- Status: complete
- Confidence: verified callback/context stores, interrupt mask, delegated return,
  and absence of direct code xrefs.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature:
  `u32 register_lp_int(callback(u64, u64), uintptr_t context)`.

## Semantics

Stores the callback in `lp_isr`, stores the shared context in `pon_int_info`,
then returns `pon_int_enable(0x40)`.

## Caller Context

No direct code xrefs target this entry; it is an external registration API.

## Evidence

- Complete ARM64 body at `0x1094` through `0x10b8`.
- Exact 64-bit stores, literal mask `0x40`, delegated return, and no direct
  xrefs.
- IDA type at `0x1094` updated to the recovered callback signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
