# 0x0fc64 buf_fifo_free_data

## Status

- Status: complete
- Confidence: verified context split, 32-byte FIFO layout, per-CPU staging,
  full/no-room paths, batch/direct enqueue, counter offsets, return paths, and
  all direct callers. Field labels are analyst names.
- Size: `0x260` bytes, 152 ARM64 instructions.
- Recovered signature:
  `u32 buf_fifo_free_data(struct fifo_staging *staging, u32 selection, void *object)`.

## Semantics

The function returns an incremented raw counter selected by execution context:
the high-context path returns counter offset `+0x18` and the IRQ-saved path
returns offset `+0x1c`, both in a 64-byte `buf_fifo_cnt[selection]` record.

Each 32-byte `buf_fifo[selection]` record has verified raw fields:

- `+0x0`: producer index.
- `+0x4`: consumer index.
- `+0x8`: ring mask.
- `+0x10`: raw lock word, released by byte-width store-release.
- `+0x18`: pointer to ring entries.

No bounds check validates `selection`, staging count, ring mask, or entry
pointer.

### High-Context Path

When `*(u32 *)(SP_EL0 + 0x10) & 0xff00` is nonzero, the function stores the
object in `staging->objects[staging->count++]`. This per-CPU staging record has
64 pointer slots and its count at `+0x200`.

On count above 31 it locks the selected FIFO without changing IRQ state. If
`mask + consumer - producer + 1` is at most 31, it unlocks, increments counter
`+0xc`, and releases exactly staging entries 0 through 31 with
`_buf_fifo_free_data(selection, entry)`. Otherwise it copies exactly 32 staged
pointers into the ring at `producer & mask`, wrapping with a second copy if
needed, advances producer by 32, unlocks, and increments counter `+0x14`. It
then clears the staging count. Every high-context call increments counter
`+0x18` before return.

### IRQ-Saved Path

When that context predicate is zero, the function saves DAIF, locks the FIFO,
and treats `mask + consumer - producer == 0xffffffff` as full. A full ring is
unlocked/restored before counter `+0xc` increments and the object is passed to
`_buf_fifo_free_data`. A non-full ring stores the object at
`entries[producer & mask]`, increments producer and counter `+0x10`, then
unlocks/restores. Every path increments counter `+0x1c` before returning.

## Caller Context

Three direct callers supply selection values 0 through 3:

- `_idm_skb_stack_push @ 0xfec4` selects 2 or 3 and uses per-CPU staging.
- `idm_skb_stack_push @ 0xffd8` selects 0 after its buffer free wrapper.
- `net_free_kmem @ 0x103ac` selects 1.

The raw selection determines the overflow release primitive in
`_buf_fifo_free_data`; ownership semantics remain selection-specific and
unresolved.

## Concurrency and Ownership

The high path assumes staging is context-local and uses only the FIFO lock for
the batch commit. The low path pairs DAIF save/restore with the same FIFO lock.
Both release only the lock's low byte with release semantics. A full/no-room
condition transfers the object to the selection-specific release helper rather
than retaining it in the FIFO.

## Evidence

- Complete 152-instruction ARM64 body at `0xfc64` through `0xfec0`.
- Exact producer/consumer/mask arithmetic, batch copy lengths, wrap path, and
  store-release unlocks.
- Three direct caller xrefs plus direct analysis of staging/fallback callers.
- Paired lock, IRQ save/restore, and selection-release helper reconstructions.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Original FIFO/staging type names and all counter meanings.
- Why the high-context path batches 32 entries while the low path inserts one.
- Exact ownership contracts for selection values 0 through 3.
