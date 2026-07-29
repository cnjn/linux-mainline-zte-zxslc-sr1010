# 0x0fba8 do_raw_spin_lock_flags.isra.2

## Status

- Status: complete
- Confidence: verified raw lock fast/slow paths, fixed slow-path arguments,
  semantically unused residual return register, and both FIFO callers.
- Size: `0x3c` bytes, 15 ARM64 instructions.
- Recovered signature:
  `void do_raw_spin_lock_flags_isra_2(raw_spinlock_t *lock)`.

## Semantics

Prefetches the supplied 32-bit lock, then performs acquire `LDAXR`/`STXR`
acquisition. A zero observed word is changed to one; an exclusive-store failure
retries from the load. A nonzero observed word calls:

```c
queued_spin_lock_slowpath(lock, observed_word, 0, 1);
```

The fast path preserves the lock pointer in the raw return register, while the
slow path leaves the callee's return register. Neither FIFO caller uses that
register, so the semantic return type is void. This helper does not change IRQ
state itself.

## Caller Context

`buf_fifo_free_data @ 0xfc64` and `buf_fifo_alloc_data @ 0x1003c` call it only
on their IRQ-saved path. They select a lock at `buf_fifo + 32 * pool + 16`,
release its low byte with store-release after FIFO mutation, then call
`arch_local_irq_restore_0` with their saved DAIF value.

## Globals and Concurrency

The function has no direct global access, allocation, callback, MMIO, or
ownership behavior. It provides acquire semantics for the supplied lock; the
caller owns release and IRQ restoration.

## Evidence

- Complete 15-instruction ARM64 body at `0xfba8` through `0xfbe0`.
- Exact `PRFM`, `LDAXR`, `STXR`, and queued slow-path argument setup.
- Two direct FIFO caller xrefs plus their save/lock/release/restore sequences.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Exact lock field type at FIFO record offset `+0x10`.
- Kernel slow-path ABI semantics for fixed arguments zero and one.
