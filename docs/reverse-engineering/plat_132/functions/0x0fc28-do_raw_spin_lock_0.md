# 0x0fc28 do_raw_spin_lock_0

## Status

- Status: complete
- Confidence: verified raw lock fast/slow paths, fixed slow-path arguments,
  semantically unused residual return register, and both FIFO callers.
- Size: `0x3c` bytes, 15 ARM64 instructions.
- Recovered signature: `void do_raw_spin_lock_0(raw_spinlock_t *lock)`.

## Semantics

This entry is machine-code-equivalent to `do_raw_spin_lock_flags.isra.2 @
0xfba8`: it prefetches a 32-bit lock, acquires zero-valued words through
acquire `LDAXR`/`STXR`, retries exclusive-store loss, and calls
`queued_spin_lock_slowpath(lock, observed_word, 0, 1)` when the observed word is
nonzero. It does not alter IRQ state.

Fast acquisition preserves the lock pointer in the raw return register, while
the slow path leaves the callee's return register. Neither direct caller uses
that value, so the semantic return type is void.

## Caller Context

`buf_fifo_free_data @ 0xfc64` and `buf_fifo_alloc_data @ 0x1003c` use this
entry on their alternate path selected by a nonzero raw context predicate at
`SP_EL0 + 0x10`. They release the FIFO record lock with a low-byte store-release
and do not use the IRQ save/restore pair on this path.

## Globals and Concurrency

No direct global access, allocation, MMIO, callback, or ownership behavior. It
provides acquire semantics for the supplied lock; callers own release.

## Evidence

- Complete 15-instruction ARM64 body at `0xfc28` through `0xfc60`.
- Exact `PRFM`, `LDAXR`, `STXR`, and queued slow-path setup.
- Two direct FIFO caller xrefs and caller-side context split.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Semantics of the raw `SP_EL0 + 0x10` context predicate.
- Exact FIFO lock field type and slow-path ABI details.
