# 0x1158c timer_int_handler

## Status

- Status: complete
- Confidence: verified IRQ masking, tasklet state transition, scheduling, and
  constant return.
- Size: `0x88` bytes, 34 ARM64 instructions.
- Recovered signature: `irqreturn_t timer_int_handler(int irq, void *dev_id)`.

## Semantics

Ignores its incoming IRQ and context, calls the vendor `disable_irq_nosync`
entry with global `g_timer0_irq_2544` and observed second argument zero, then
sets tasklet state bit zero with an exclusive load/store loop. A successful
zero-to-one transition performs the observed barrier and calls
`__tasklet_hi_schedule(&timer0_tasklet)`. Already-scheduled tasklets are not
queued again. It always returns one.

No timer MMIO acknowledgement occurs here; `timer0_process` later re-enables the
IRQ.

## Evidence

The request-IRQ path materializes this function pointer. Complete body:
`0x1158c` through `0x11610`; tasklet state is raw `timer0_tasklet + 0x08`.

## Source-Like Reconstruction

`recovered/plat_timer.c`.
