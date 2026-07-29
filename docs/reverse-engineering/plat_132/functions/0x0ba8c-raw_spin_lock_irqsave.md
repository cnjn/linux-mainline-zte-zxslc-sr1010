# 0x0ba8c __raw_spin_lock_irqsave

## Status

- Status: complete
- Confidence: verified DAIF save/mask sequence, raw lock fast/slow paths,
  slow-path arguments, return value, and all direct callers.
- Size: `0x58` bytes, 22 ARM64 instructions.
- Recovered signature:
  `unsigned long __raw_spin_lock_irqsave(raw_spinlock_t *lock)`.

## Semantics

The helper reads the full `DAIF` system register and returns that exact original
value. If DAIF bit 7 is clear, it executes `MSR DAIFSet, #2`, masking local IRQs;
an already-set bit is left set without another write.

It then prefetches the supplied 32-bit lock and attempts acquisition with an
acquire `LDAXR` followed by `STXR 1`. Store-exclusive failure retries from the
load. A nonzero observed lock word bypasses the store and calls:

```c
queued_spin_lock_slowpath(lock, observed_word, 0, 1);
```

On either successful path it returns the saved DAIF value. It never restores
interrupt state itself; the caller unlocks first and then restores the returned
state through the paired IRQ-restore helper.

## Caller Context

Five direct call sites in `idm_net_tx @ 0xd234` and `cpu_net_tx @ 0xd668` use
this path when their CPU-context predicate selects irqsave locking. Recovered
TX code keeps the returned flags in `cpu_tx_lock_state` and invokes
`arch_local_irq_restore(flags)` after byte-width store-release unlock.

## Globals and Concurrency

No module globals, MMIO, allocation, or ownership transfer. It changes only
CPU-local interrupt masking and acquires the supplied lock. The slow path owns
the contention protocol.

## Evidence

- Complete 22-instruction ARM64 body at `0xba8c` through `0xbae0`.
- Exact `MRS DAIF`, bit-7 test, `MSR DAIFSet, #2`, `PRFM`, `LDAXR`, `STXR`, and
  slow-path register setup.
- Five direct caller xrefs plus matching lock/unlock code in both TX paths.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Exact vendor meaning of the caller's `SP_EL0 + 0x10` context predicate.
- Kernel slow-path ABI semantics for fixed arguments zero and one.
