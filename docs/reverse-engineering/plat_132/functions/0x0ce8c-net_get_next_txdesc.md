# 0x0ce8c net_get_next_txdesc

## Status

- Status: complete
- Confidence: verified threshold branch, completion/full behavior, producer
  wrap arithmetic, fixed descriptor stride, globals, and all direct callers.
- Size: `0x88` bytes, 33 ARM64 instructions.
- Recovered signature: `void *net_get_next_txdesc(zte_tx_queue_t *queue)`.

## Semantics

This is a reservation helper, not a submission helper. It returns the current
32-byte TX descriptor and advances queue producer word `+0x10` modulo depth
word `+0x20`. It does not change pending word `+0x14`, write an owner slot, or
notify the IDM backend.

```c
if (queue->pending >= (uint16_t)g_net_check_threshold) {
    net_check_tx_done_nolock(queue);
    if (queue->pending >= queue->depth) {
        ++net_tx_full;
        return NULL;
    }
}

slot = queue->producer;
queue->producer = (slot + 1 == queue->depth) ? 0 : slot + 1;
return queue->descriptor_base + 32 * slot;
```

The branch intentionally does not test depth when pending is below the 16-bit
threshold. Correctness in that case relies on the threshold/depth configuration
and caller serialization; the function has no defensive validation.

## Caller Context

- `cpu_net_nb_tx @ 0x0cf14` reserves batched unlock-queue descriptors.
- `idm_net_tx @ 0x0d234` reserves the IDM/Wi-Fi descriptor.
- `cpu_net_tx @ 0x0d668` uses it for SW, PON, and OMCI/OAM direct TX.
- `net_gso_upload_send @ 0x0e634` and both normal-GSO descriptor paths in
  `net_tcp_gso_tx @ 0x0f258` reserve CPU TX descriptors.

Submit failures in the direct TX callers invoke `net_set_prev_txdesc` to undo
and issue their backend update or doorbell.

## Concurrency and Ownership

- No local lock or memory barrier.
- No descriptor ownership changes occur at reservation time.
- Callers must serialize producer, pending, completion reclaim, and any rollback.

## Evidence

- Complete 33-instruction ARM64 body at `0xce8c` through `0xcf10`.
- Eight direct code xrefs across six caller functions.
- Completion behavior cross-checked with `net_check_tx_done_nolock @ 0x0b4fc`.
- Rollback cross-checked with `net_set_prev_txdesc @ 0x0aff8`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Why `g_net_check_threshold` has the observed 16-bit, configuration-dependent
  gating behavior, and whether any runtime configuration can violate the implied
  pending/depth invariant.
