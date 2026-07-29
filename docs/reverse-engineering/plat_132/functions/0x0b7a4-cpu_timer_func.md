# 0x0b7a4 cpu_timer_func

## Status

- Status: complete
- Confidence: verified queue gate, lock/reclaim order, reorder call, timer
  rearm target, callback argument behavior, and registration xrefs.
- Size: `0xc8` bytes, 50 ARM64 instructions.
- Recovered signature: `void cpu_timer_func(struct timer_list *timer)`.

## Semantics

The timer callback ignores its passed timer pointer and operates on global
`cpu_net_timer`. It reads the low 16 bits of `g_net_check_threshold`.

When the value is greater than one, it performs the following in order:

1. Acquires `omcioam_lock_tx`, calls `net_check_tx_done_nolock(omcioam_tq)`,
   then releases the lock with `STLRB`.
2. Acquires `net_lock_tx`, calls `net_check_tx_done_nolock(cpu_tq)`, then
   releases it the same way.

It always then acquires `idm_lock_tx`; when the threshold condition holds, it
reclaims `idm_tq`; it always calls `net_check_reorder_rls_nolock` while holding
that lock; then releases it with `STLRB`.

Finally it stores `jiffies + 1` to the global timer expiry and calls
`add_timer_on(&cpu_net_timer, 0)`. The residual `add_timer_on` result in `x0`
is not a semantic return value for this timer callback.

## Concurrency

All three reclaim calls occur under their matching raw TX locks. The reorder
release operation shares `idm_lock_tx` with `cpu_rls_poll`, avoiding concurrent
reorder release from the timer and NAPI route. The threshold gate suppresses
completion reclamation but not the reorder pass or timer rearm.

## Evidence

- Complete 50-instruction ARM64 disassembly at `0xb7a4` through `0xb868`.
- `cpu_net_init @ 0xe220` initializes and schedules this callback on CPU 0.
- Direct decompilation of `cpu_timer_unlock @ 0xb700`, which uses the same
  reclaim primitive for per-unlock timers.
- Shared reclamation implementation at `net_check_tx_done_nolock @ 0xb4fc`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- The intended threshold semantics and why it is read as 16 bits are unknown.
- `cpu_timer_unlock` lock context and its relationship to unlock queue aliases
  require a separate function record.
