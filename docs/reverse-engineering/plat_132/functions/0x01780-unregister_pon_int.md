# 0x01780 unregister_pon_int

## Status

- Status: complete
- Confidence: verified IRQ source, context address, void free call, and sole
  direct caller.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature: `void unregister_pon_int(void)`.

## Semantics

Calls `free_irq(g_pon_irq, &pon_int_info)` and returns.

## Caller Context

Its sole direct caller is `zx_pon_remove @ 0x308`.

## Evidence

- Complete ARM64 body at `0x1780` through `0x17a4`.
- Exact global IRQ load, context address `&pon_int_info`, void kernel API call,
  and caller xref.
- IDA type at `0x1780` updated to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
