# 0x0fbe4 _buf_fifo_free_data

## Status

- Status: complete
- Confidence: verified selection dispatch, imported targets/cache global, fixed
  free reason, raw residual returns, and all direct callers.
- Size: `0x44` bytes, 17 ARM64 instructions.
- Recovered signature: `void _buf_fifo_free_data(u32 selection, void *object)`.

## Semantics

Dispatches one object to a release primitive selected by the raw first argument:

| Selection | Action |
| --- | --- |
| `0` | `kfree_skb_without_data(object)` |
| `1` | `kmem_cache_free(kmem_buf_cache, object)` |
| other | `__dev_kfree_skb_any(object, 1)` |

The function has no local validation, lock, allocation, or ownership policy
beyond choosing the release path. Imported helpers leave raw return-register
values, but all three in-module callers ignore them; semantic return type is
void.

## Caller Context

Three direct calls occur in `buf_fifo_free_data @ 0xfc64` and
`buf_fifo_rls @ 0x10414`. Both supply a raw FIFO-related selection and an
object dequeued or drained from FIFO state; the exact selection-to-object
ownership contract remains unresolved.

## Globals and Concurrency

Reads `kmem_buf_cache` only for selection 1. It does not itself synchronize
access to FIFO state or protect object lifetime.

## Evidence

- Complete 17-instruction ARM64 body at `0xfbe4` through `0xfc24`.
- Exact branch comparisons for zero and one, plus all three imported targets.
- Three direct caller xrefs and paired IDM cache initialization evidence.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Exact semantic names and ownership contracts for each selection value.
- Meaning of fixed `__dev_kfree_skb_any` reason value one in this vendor kernel.
