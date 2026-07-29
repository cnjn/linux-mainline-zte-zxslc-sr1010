# 0x1003c buf_fifo_alloc_data

## Status

- Status: complete
- Confidence: verified context split, FIFO producer/consumer/mask use, high
  staging layout, batch/direct dequeue, counter updates, null handling, and all
  direct callers. Field labels are analyst names.
- Size: `0x260` bytes, 152 ARM64 instructions.
- Recovered signature:
  `void *buf_fifo_alloc_data(struct fifo_alloc_staging *staging, u32 selection)`.

## Semantics

Consumes one raw object from `buf_fifo[selection]`, returning null when no
object is available or when a consumed/staged slot itself is null. It uses the
same unchecked 32-byte FIFO layout as `buf_fifo_free_data`: producer `+0x0`,
consumer `+0x4`, mask `+0x8`, lock `+0x10`, and entry pointer `+0x18`.

### High-Context Path

When `*(u32 *)(SP_EL0 + 0x10) & 0xff00` is nonzero, it uses an allocation
staging record with pointer slots at `+0x100..+0x2ff` and count at `+0x204`.
If that count is zero, it locks the FIFO without changing IRQ state:

- An empty producer/consumer pair increments counter `+0x0`, unlocks, and
  leaves staging empty.
- Otherwise it copies `min(producer - consumer, 32)` entries beginning at
  `consumer & mask`, wraps if needed, advances consumer by the copied count,
  increments counter `+0x4`, unlocks, and publishes the staging count.

It then decrements the staging count, reads that slot, and increments counter
`+0x20`. A non-null slot returns immediately. An empty staging record or null
slot increments counter `+0x28` and returns null.

### IRQ-Saved Path

When the predicate is zero, the function saves DAIF, locks the FIFO, then:

- Equal producer/consumer increments counter `+0x0` and produces null.
- Otherwise it reads `entries[consumer & mask]`, increments consumer and
  counter `+0x8`.

It always byte-release unlocks and restores DAIF, then increments counter
`+0x24`. A null result also increments counter `+0x28`; otherwise it returns the
object.

No bounds check validates selection, staging count, mask, entry pointer, or
returned object type.

## Caller Context

Four direct callers consume selection-specific FIFO objects:

- `idm_skb_stack_pop @ 0x1029c`.
- `net_alloc_skb @ 0x10354`.
- `net_alloc_kmem @ 0x10380`.
- `buf_fifo_rls @ 0x10414`.

Those callers establish object interpretation and ownership after a non-null
return; this helper only moves raw pointers.

## Concurrency and Ownership

The high path assumes staging is context-local and batches at most 32 entries
under the FIFO lock. The low path uses DAIF save/restore around direct dequeue.
Both release only the lock's low byte with release semantics. No object is freed
here.

## Evidence

- Complete 152-instruction ARM64 body at `0x1003c` through `0x10298`.
- Exact producer/consumer arithmetic, batch copy/wrap lengths, staging offsets,
  counter offsets, and release ordering.
- Four direct caller xrefs and paired FIFO enqueue/lock/IRQ helper analysis.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Original allocation-staging type name and purpose of its opaque first 0x100
  bytes.
- Complete meanings of FIFO counters and null entry slots.
- Object ownership contracts at each caller.
