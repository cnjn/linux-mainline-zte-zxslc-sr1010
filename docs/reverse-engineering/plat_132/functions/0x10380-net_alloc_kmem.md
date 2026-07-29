# 0x10380 net_alloc_kmem

## Status

- Status: complete
- Confidence: verified per-CPU staging selection, fixed FIFO selection, return
  propagation, and sole direct caller.
- Size: `0x2c` bytes, 10 ARM64 instructions.
- Recovered signature: `void *net_alloc_kmem(void)`.

## Semantics

Adds raw `TPIDR_EL1` to the global `kmem_free_data` per-CPU staging base and
returns the unchanged result of `buf_fifo_alloc_data(staging, 1)`. It has no
fallback allocator, validation, lock, counter, or ownership policy of its own.
A null FIFO result propagates unchanged.

## Caller Context

`idm_alloc_buf @ 0x13df8` is the sole direct caller. That IDM backend function
interprets the result as a buffer allocation candidate and defines subsequent
buffer initialization/ownership behavior.

## Evidence

- Complete ten-instruction ARM64 body at `0x10380` through `0x103a8`.
- Exact `kmem_free_data + TPIDR_EL1` computation and fixed FIFO selection one.
- Sole direct caller xref in `idm_alloc_buf`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Original per-CPU staging type and object ownership when FIFO 1 is empty.
