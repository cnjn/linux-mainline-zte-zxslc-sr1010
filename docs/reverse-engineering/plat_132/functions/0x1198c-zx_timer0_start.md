# 0x1198c zx_timer0_start

## Status

- Status: complete
- Confidence: verified initialization gate, callback policy, configuration,
  affinity expression, start order, and exported interface.
- Size: `0x80` bytes, 32 ARM64 instructions.
- Recovered signature: `void zx_timer0_start(u32 rate, u32 cpu, timer0_cb_t cb)`.

## Semantics

Calls `zx_timer_init` only when global IRQ value is zero, ignoring that function's
residual outcome. It stops Timer0, replaces `timer0_func` only for a non-null
callback, configures the dothz divisor, calls `irq_set_affinity_hint` with the
exact `cpu_bit_bitmap` expression derived from `cpu`, then starts Timer0.

Neither failed initialization nor failed affinity setup aborts later steps. A
zero rate can leave the previous reload setting intact while the timer is still
started. The entry is module-exported and has no direct IDA code caller.

## Source-Like Reconstruction

`recovered/plat_timer.c`.
