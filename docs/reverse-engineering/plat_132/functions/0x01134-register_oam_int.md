# 0x01134 register_oam_int

## Status

- Status: complete
- Confidence: verified callback store, unchanged shared context, NPPT mask,
  delegated return, and absence of direct code xrefs.
- Size: `0x20` bytes, 8 ARM64 instructions.
- Recovered signature: `u32 register_oam_int(callback(u64, u64))`.

## Semantics

Stores the callback in `oam_isr`, leaves `pon_int_info` unchanged, then returns
`nppt_int_enable(0x100)`.

## Caller Context

No direct code xrefs target this entry; it is an external registration API.

## Evidence

- Complete ARM64 body at `0x1134` through `0x1150`.
- Single callback store, literal mask `0x100`, delegated NPPT return, and no
  context store.
- IDA type at `0x1134` updated to the recovered callback signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
