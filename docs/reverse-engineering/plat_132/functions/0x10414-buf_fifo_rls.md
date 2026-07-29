# 0x10414 buf_fifo_rls

## Status

- Status: complete
- Confidence: verified selection mapping, per-CPU staging use, signed drain
  bound, dequeue/free ordering, and callers; selection ownership labels remain
  inherited from the established FIFO helpers.
- Size: `0xb4` bytes, 44 ARM64 instructions.
- Recovered signature: `void buf_fifo_rls(u32 selection)`.

## Role

Drain up to one ring capacity of raw objects from one selected FIFO and send each
object through the existing selection-specific release helper.

## Semantics

Inputs above three return immediately. Valid selections map to per-CPU staging
bases as follows:

| Selection | Staging base |
| --- | --- |
| `0` | `skb_free_data` |
| `1` | `kmem_free_data` |
| `2` | `wifi0_free_data` |
| `3` | `wifi1_free_data` |

The function adds `__my_cpu_offset()` to that base, reads the selected FIFO mask,
and attempts at most signed `(s32)(mask + 1)` iterations. Each iteration calls
`buf_fifo_alloc_data(staging, selection)`; null stops the loop, while a non-null
object is immediately passed to `_buf_fifo_free_data(selection, object)`.

There is no local lock, allocation, error report, or explicit return value. FIFO
locking and staged dequeue behavior remain delegated to `buf_fifo_alloc_data`.

## Caller Context

`buf_fifo_rls_all @ 0x104c8` is the only direct caller and invokes all four
selections in fixed order. Runtime kallsyms marks this function module-local.

## Evidence

- Complete ARM64 body at `0x10414` through `0x104c4`.
- Unsigned selection gate, four staging-base branches, TPIDR_EL1 helper call,
  mask-plus-one signed loop condition, and paired allocation/free calls.
- Four direct xrefs from the all-selection wrapper.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Why this drain helper has no observed direct lifecycle caller beyond the
  unreferenced all-selection wrapper.
