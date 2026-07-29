# 0x103ac net_free_kmem

## Status

- Status: complete
- Confidence: verified object-argument forwarding, per-CPU staging selection,
  fixed FIFO selection, return propagation, and sole direct caller.
- Size: `0x30` bytes, 12 ARM64 instructions.
- Recovered signature: `u32 net_free_kmem(void *object)`.

## Semantics

Preserves its object argument, adds raw `TPIDR_EL1` to the global
`kmem_free_data` staging base, and returns the unchanged result of:

```c
buf_fifo_free_data(staging, 1, object)
```

It has no local validation, lock, release decision, counter, or ownership policy
beyond forwarding to the FIFO helper.

## Caller Context

`idm_free_buf @ 0x143c4` is the sole direct caller. Its buffer-return path uses
this wrapper for selection-1 objects; the backend establishes the object range
and subsequent ownership behavior.

## Evidence

- Complete 12-instruction ARM64 body at `0x103ac` through `0x103d8`.
- Exact preservation of input `x0` into `x2`, `kmem_free_data + TPIDR_EL1`
  staging computation, and fixed selection one.
- Sole direct caller xref in `idm_free_buf`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Original staging type and the semantic meaning of the propagated raw counter.
