# 0x00fa8 register_gmac_int

## Status

- Status: complete
- Confidence: verified callback/context stores, interrupt mask, delegated return,
  and absence of direct code xrefs.
- Size: `0x24` bytes, 9 ARM64 instructions.
- Recovered signature:
  `u32 register_gmac_int(callback(u64, u64), uintptr_t context)`.

## Semantics

Stores the callback and context into `gpon_isr` and `pon_int_info`, then returns
`pon_int_enable(1)`.

## Caller Context

No direct code xrefs target this entry; it is an external registration API.

## Evidence

- Complete ARM64 body at `0xfa8` through `0xfc8`.
- Paired 64-bit global store, literal mask one, delegated return, and no direct
  xrefs.
- IDA type at `0xfa8` updated to the recovered callback signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
