# 0x0b768 do_raw_spin_lock

## Status

- Status: complete
- Confidence: verified prefetch, acquire/exclusive fast path, contention test,
  slow-path arguments, callers, and semantically unused residual return value.
- Size: `0x3c` bytes, 15 ARM64 instructions.
- Recovered signature: `void do_raw_spin_lock(volatile u32 *lock)`.

## Semantics

Performs a store-prefetch for the lock word, then repeatedly issues `LDAXR` and
attempts `STXR lock = 1` only when the acquired word is zero. An exclusive-store
failure retries the load/exclusive sequence. A nonzero observed word takes the
kernel slow path exactly as:

```c
queued_spin_lock_slowpath(lock, observed, 0, 1);
```

The helper has no null check, IRQ masking, allocation, local unlock, or semantic
return value. The direct fast-path equivalent is an acquire compare-exchange from
zero to one; the `__ATOMIC_ACQUIRE` form preserves the observed `LDAXR`/`STXR`
ordering.

## Caller Context

There are nine direct in-module call sites. `cpu_timer_func` acquires the three
TX locks before reclaim/reorder work; `cpu_rls_poll` acquires `idm_lock_tx`; TX
paths in `cpu_net_tx` and `idm_net_tx` acquire their queue locks. Their paired
releases use low-byte store-release operations or the recovered TX unlock path.

## Concurrency and Ownership

- This is the acquisition primitive itself; it obtains no unrelated resource.
- Correct use requires the caller to release the exact lock with compatible
  release ordering.
- The slow path is imported from the vendor kernel and receives the raw observed
  lock word without transformation.

## Evidence

- Complete 15-instruction ARM64 body at `0xb768` through `0xb7a0`.
- `PRFM #0x11`, `LDAXR`, and `STXR` establish the fast-path protocol.
- `queued_spin_lock_slowpath(lock, observed, 0, 1)` direct import at `0xb794`.
- Nine direct IDA callers across TX, timer, and NAPI release paths.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Exact vendor-kernel interpretation of slow-path arguments 2 and 3.
- Whether all external callers use the same low-byte store-release unlock ABI.
