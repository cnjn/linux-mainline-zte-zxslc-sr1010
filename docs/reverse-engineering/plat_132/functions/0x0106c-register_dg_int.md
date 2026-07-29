# 0x0106c register_dg_int

## Status

- Status: complete
- Confidence: verified opaque handler/context stores, interrupt mask, delegated
  return, and absence of direct code xrefs.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature: `u32 register_dg_int(uintptr_t handler, uintptr_t context)`.

## Semantics

Stores the opaque handler word in `dg_isr`, stores the shared context in
`pon_int_info`, then returns `pon_int_enable(0x20)`.

## Caller Context

No direct code xrefs target this entry; it is an external registration API.
The registered `dg_isr` word is not dispatched by this module's PON ISR, so its
callable prototype cannot be established internally.

## Evidence

- Complete ARM64 body at `0x106c` through `0x1090`.
- Exact 64-bit stores, literal mask `0x20`, delegated return, and no direct
  xrefs.
- IDA type at `0x106c` updated to the evidence-limited opaque-word signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
