# 0x0aff8 net_set_prev_txdesc

## Status

- Status: complete
- Confidence: verified full body, producer/depth fields, all callers, and
  caller failure context.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `void net_set_prev_txdesc(zte_tx_queue_t *queue)`.

## Semantics

Undo one prior `net_get_next_txdesc` producer advance:

```c
if (queue->producer == 0)
    queue->producer = queue->depth;
--queue->producer;
```

The actual machine return leaves the input queue pointer in `x0`, but every
direct caller ignores it, so the recovered semantic return type is `void`.
There is no validation: a zero depth with zero producer underflows producer to
`UINT32_MAX`.

## Caller Context

- `idm_net_tx @ 0x0d234` rolls back after `idm_wifi_tx` reports failure.
- `cpu_net_tx @ 0x0d668` rolls back after failed SW, PON, or OMCI/OAM backend
  submission.

It is not used by GSO owner-ring handoff paths because their observed backend
update helper returns zero unconditionally.

## Concurrency and Ownership

- No local lock, barrier, allocation, callback, or ownership mutation.
- Callers invoke it while holding their matching TX serialization lock.
- It only returns the descriptor slot to the producer sequence; descriptor data
  and owner/pending state are untouched.

## Evidence

- Complete six-instruction ARM64 body at `0xaff8` through `0xb00c`.
- Four direct code xrefs: one IDM TX and three CPU logical-port TX failure paths.
- Complementary reservation behavior at `net_get_next_txdesc @ 0x0ce8c`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- None beyond the shared queue ABI and caller locking contract.
