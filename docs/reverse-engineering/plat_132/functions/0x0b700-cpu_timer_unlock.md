# 0x0b700 cpu_timer_unlock

## Status

- Status: complete
- Confidence: verified timer-index arithmetic, queue selection, reclaim call,
  expiry update, target CPU, rearm behavior, and registration context; timer
  layout labels are strong inference.
- Size: `0x68` bytes, 24 ARM64 instructions.
- Recovered signature: `void cpu_timer_unlock(zte_timer_t *timer)`.

## Semantics

Derives a timer index from the passed pointer relative to the 40-byte
`cpu_unlock_timer[2]` array, then performs:

```c
index = timer - cpu_unlock_timer;
net_check_tx_done_nolock(unlock_tq[index]);
cpu_unlock_timer[index].expires = jiffies + 1;
add_timer_on(&cpu_unlock_timer[index], ipsec_tx_cpu);
```

The ARM64 multiply by `0xcccccccd` implements division by the five 8-byte units
in each timer element. The callback canonicalizes its timer pointer from the
derived index before updating/rearming it. The residual `add_timer_on` return is
not used as a semantic return value.

## Caller Context

There are no direct code-call xrefs because this is a timer callback.
`cpu_net_init @ 0x0e220` initializes both `cpu_unlock_timer[0]` and `[1]` with
this function, sets each expiry to `jiffies + 1`, and schedules each on
`ipsec_tx_cpu`. Initialization obtains `unlock_tq[0]` from IDM TX queue index 3,
aliases `unlock_tq[1]` to it, and checks only the first pointer for null.

## Concurrency and Ownership

- No local lock, IRQ-state management, null check, or allocation.
- Calls the explicitly nolock completion reclaimer. Timer serialization and any
  queue ownership synchronization are external obligations.
- A non-array or out-of-range timer pointer produces an unchecked derived index.

## Evidence

- Complete 24-instruction ARM64 body at `0xb700` through `0xb764`.
- Two data xrefs from the timer-function slots initialized by `cpu_net_init`.
- Direct `net_check_tx_done_nolock`, `jiffies`, `add_timer_on`, and
  `ipsec_tx_cpu` references.
- Cross-check with the verified 40-byte timer layout and unlock-queue setup.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Why the two timer slots alias the same queue in the observed CPU-net setup.
- External serialization of these callback-driven completion scans.
