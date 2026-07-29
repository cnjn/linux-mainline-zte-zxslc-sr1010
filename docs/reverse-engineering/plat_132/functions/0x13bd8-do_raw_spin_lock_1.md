# 0x13bd8 do_raw_spin_lock_1

## Status

- Status: complete
- Confidence: verified pointer ABI, write prefetch, exclusive acquire/retry,
  contended slow path, and all direct callers.
- Size: `0x3c` bytes, 15 ARM64 instructions.
- Recovered signature: `void do_raw_spin_lock_1(u32 *lock)`.

## Semantics

Acquires the supplied queued spinlock pointer. It performs `PRFM` for write,
then retries an `LDAXR`/`STXR` transition from zero to one. A nonzero observed
word goes to `queued_spin_lock_slowpath(lock, observed, 0, 1)`. The pointer left
in `X0` on an uncontended return is not a semantic result.

## Caller Context

Direct callers include the refill flush/reuse paths, allocator/free/FIFO paths,

## Source-Like Reconstruction

`recovered/plat_idm.c`.
