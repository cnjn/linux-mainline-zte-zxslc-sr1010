# 0x0dfa8 cpu_net_nb_desc_tx

## Status

- Status: complete
- Confidence: verified owner-slot arithmetic, tag/pending/ops update order,
  return value, and both callers; CPU TX queue field labels are strong inference.
- Size: `0x5c` bytes, 23 ARM64 instructions.
- Recovered signature: `int cpu_net_nb_desc_tx(void *nbuf, void *descriptor)`.

## Semantics

The function uses the global CPU TX queue. It computes:

```c
slot = (descriptor - queue->descriptor_base) >> 5;
queue->owners[slot] = (uintptr_t)nbuf | 1;
++queue->pending;
cpu_net_ops->update_tx_queue(queue->queue_id, 1);
```

It always returns zero and does not validate the queue, descriptor range, owner
slot, nbuf pointer, or update callback. Bit 0 is the tagged-nbuf convention
consumed by `net_check_tx_done_nolock` before it calls `cpu_net_free_nbuf`.

## Caller Context

`net_gso_upload_send @ 0x0e634` and `net_tcp_gso_tx @ 0x0f258` call this after
successful GSO descriptor construction and barrier/setup. Their negative-return
checks are defensive because this helper returns zero unconditionally.

## Concurrency and Ownership

- No local lock; parent GSO paths run under TX serialization.
- Successful call transfers nbuf ownership to the CPU TX owner ring until
  completion reclaims the tagged entry.

## Evidence

- Complete 23-instruction ARM64 body at `0xdfa8` through `0xe000`.
- Two direct callers and direct owner/pending/ops field setup.
- Cross-check with tagged nbuf reclaim in `net_check_tx_done_nolock`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Original CPU TX queue type and exact queue-id/doorbell semantics at ops `+0x70`.
