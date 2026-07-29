# 0x1154c timer0_process

## Status

- Status: complete
- Confidence: verified tasklet callback flow, IRQ re-enable, counter update, and
  semantic void ABI.
- Size: `0x40` bytes, 16 ARM64 instructions.
- Recovered signature: `void timer0_process(unsigned long unused)`.

## Semantics

If `timer0_func` is non-null, invokes it with no arguments and ignores any
residual result. It then calls `enable_irq(g_timer0_irq_2544)` and increments
the 32-bit `timer0_int_cnt`. The incoming tasklet data argument is unused.

## Concurrency and Evidence

This is the function pointer stored at `timer0_tasklet + 0x18`; it runs after
the hard handler disables the IRQ and schedules the high-priority tasklet. No
local synchronization protects callback publication or the counter. Complete
body: `0x1154c` through `0x11588`.

## Source-Like Reconstruction

`recovered/plat_timer.c`.
