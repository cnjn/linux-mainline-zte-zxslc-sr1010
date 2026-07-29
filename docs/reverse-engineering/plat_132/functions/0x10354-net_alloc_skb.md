# 0x10354 net_alloc_skb

## Status

- Status: complete
- Confidence: verified per-CPU staging selection, fixed FIFO selection, return
  propagation, and sole direct caller.
- Size: `0x2c` bytes, 10 ARM64 instructions.
- Recovered signature: `struct sk_buff *net_alloc_skb(void)`.

## Semantics

Adds raw `TPIDR_EL1` to the global `skb_free_data` per-CPU staging base and
returns the unchanged result of `buf_fifo_alloc_data(staging, 0)`. It has no
fallback allocator, validation, lock, allocation, counter, or ownership policy
of its own. A null FIFO result propagates unchanged.

## Caller Context

`idm_net_rx @ 0xbf6c` is the sole direct caller. It uses the returned pointer as
the skb head argument to `alloc_skb_attach_buffer`; that caller establishes the
later attachment/ownership path.

## Evidence

- Complete ten-instruction ARM64 body at `0x10354` through `0x1037c`.
- Exact `skb_free_data + TPIDR_EL1` computation and fixed FIFO selection zero.
- Sole direct caller xref in IDM RX.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Original per-CPU staging type and the ownership contract when FIFO 0 is empty.
