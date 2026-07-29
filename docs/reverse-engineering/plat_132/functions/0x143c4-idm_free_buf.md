# 0x143c4 idm_free_buf

## Status

- Status: complete
- Confidence: verified boundary classification, all four status lanes, normal
  and jumbo release destinations, per-CPU batch stash layout, FIFO capacity
  test, error behavior, and ops-table role.
- Size: `0x240` bytes, 143 ARM64 instructions.
- Recovered signature: `void idm_free_buf(void *buffer, u32 pool)`.

## Semantics

Recomputes the reserved data-region virtual boundary and classifies `buffer`.
Buffers below the boundary increment `idm_status[2 * cpu + pool + 8]`; pool zero
goes to `net_free_kmem`, while nonzero pools go to `kmem_cache_free` with
`idm_jbuf_cache`.

Buffers at or above the boundary increment `idm_status[2 * cpu + pool]`. A
nonzero pool, or a zero pool with `(SP_EL0[+0x10] & 0xff00) == 0`, is submitted
directly through `idm_fifo_in(pool, buffer)`. For zero pool with a nonzero mask,
the buffer is stored in the current CPU's producer stash: 32 pointers at
`idm_free_data + 0x000`, with count at `+0x200`.

On the 32nd producer-stash entry, the function locks FIFO0. If
`mask + out - in + 1 > 31`, it copies all 32 pointers to FIFO0 with wrap-aware
one/two-copy logic, advances `in` by 32, releases the lock, and increments
`idm_fifo_in_ncnt`. If insufficient space is available, it releases the lock
and emits a rate-limited `idm_fifo_in_n` diagnostic. In both cases it clears the
producer count afterward, so an undrained full batch is discarded.

The paired consumer stash used by `idm_alloc_buf` is separate: pointers are at
`+0x100`, count at `+0x204`.

## Caller Context

The only xref is an IDM ops-table entry at `0x26708` (`+0x30`), used as the
generic CPU-net buffer-release callback.

## Evidence

- Complete ARM64 body at `0x143c4` through `0x14600`.
- Boundary/classification and status-counter updates at `0x143c8` through
  `0x145d0`.
- Direct release paths at `0x145a4` through `0x145ec`.
- Producer stash batching, FIFO capacity test/copy, and clear at `0x144a4`
  through `0x145a0`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- The semantic meaning of the reserved-boundary split is inferred from allocator
  and release behavior; vendor source type/ownership labels are unavailable.
