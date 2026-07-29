# 0x13df8 idm_alloc_buf

## Status

- Status: complete
- Confidence: verified SP_EL0/pool gate, per-CPU stash layout, bounded FIFO
  batch refill, generic FIFO path, counter updates, fallback ordering, error
  path, and all direct callers.
- Size: `0x23c` bytes, 140 ARM64 instructions.
- Recovered signature: `void *idm_alloc_buf(u32 pool)`.

## Semantics

For pool zero only, when `(*(u32 *)(SP_EL0 + 0x10) & 0xff00) != 0`, it first
uses the current CPU's `idm_free_data + __my_cpu_offset_0()` stash. The verified
stash fields are a 32-pointer array at `+0x100` and count at `+0x204`. An empty
stash is refilled from FIFO0 under its lock with at most 32 pointers, using one
or two `memcpy` calls to handle the FIFO mask wrap. Pointers are popped LIFO;
a null popped pointer falls through to fallback.

All other cases consume one pointer directly from `idm_fifo[pool]` under that
FIFO's lock. Both paths increment empty/out diagnostic counters exactly as the
binary does and release the byte lock with `STLRB` semantics. Pool is unchecked,
so nonzero values index FIFO/counter state directly.

If no non-null FIFO/stash object is available, it increments `idm_debug_cnt`.
For pool zero it first calls `net_alloc_kmem`; after that failure it allocates
from `idm_buf_cache`. Nonzero pools allocate from `idm_jbuf_cache`. Both cache
allocations use flags `2592`; a null cache result produces a rate-limited
`idm failed to alloc skb` diagnostic and returns null.

## Caller Context

Called by `idm_alloc_nbuf @ 0x14034`, `idm_rx_refill0 @ 0x14144`, and
`_idm_rx_refill @ 0x1c460`. The refill path passes its raw pool selector
without local bounds validation.

## Evidence

- Complete ARM64 body at `0x13df8` through `0x14030`.
- SP_EL0 word/mask gate at `0x13e14` through `0x13e28`.
- Per-CPU stash accesses at `+0x100/+0x204`, bounded batch copy, and FIFO0
  release at `0x13e54` through `0x13f24`.
- Generic FIFO lock/pop path at `0x13f28` through `0x13fb0`.
- `net_alloc_kmem`, cache selection/allocation, and ratelimited failure path at
  `0x13fb4` through `0x14018`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- The kernel/task meaning of the masked `SP_EL0 + 0x10` field is not established
  by this module; the exact gate is preserved without naming it as a preemption
  or interrupt state.
- The producer/consumer synchronization contract for per-CPU stash entries is
  not visible beyond the observed FIFO locks and local count manipulation.
