# 0x11794 zx_timer_init

## Status

- Status: complete
- Confidence: verified OF loop, mappings, IRQ request, all error returns, and
  tasklet/handler wiring; semantic void ABI is strong inference.
- Size: `0x1a8` bytes, 106 ARM64 instructions.
- Recovered signature: `void zx_timer_init(void)`.

## Semantics

Iterates the module's four-record OF match table and independently handles
`zxic,apb-timer0`, `zxic,apb-timer1`, and `zte,lsp0_crm`. Each recognized node
uses `of_iomap(node, 0)`; Timer0 additionally uses `of_irq_get(node, 0)` and
requires a positive result. Mapping/IRQ failure logs and returns immediately.

After the loop, all three mappings must be non-null. The function selects Timer0
clock mode one, then requests `g_timer0_irq_2544` with hard handler
`timer_int_handler`, no thread handler, flags zero, name `zx timer0`, and null
dev-id. A negative request status is logged but not propagated. There is no
mapping, node, IRQ, tasklet, callback, or affinity teardown here.

## Evidence

Complete body at `0x11794` through `0x11938`, raw match data, exact compatible
strings, OF/import calls, and materialized tasklet/handler references.

## Source-Like Reconstruction

`recovered/plat_timer.c`.

## Open Questions

- Captured runtime lacked these vendor timer nodes and IRQ handler, so hardware
  register meaning and teardown policy remain unobserved.
