# 0x0cf14 cpu_net_nb_tx

## Status

- Status: complete
- Confidence: verified list consumption, selector validation, descriptor/owner
  writes, batching, error behavior, stats paths, return behavior, export, and
  all in-module references; nbuf/descriptor field labels are strong inference.
- Size: `0x320` bytes, 198 ARM64 instructions.
- Recovered signature: `int cpu_net_nb_tx(zte_nbuf_t *head)`.

## Semantics

The function selects `unlock_tq[cpu2unlock_tq[cpu_number]]`, where `cpu_number`
is read from its per-CPU location at `TPIDR_EL1`. A selector above one is
rate-limited, logged, and returns `-1`; there is no CPU-array bound or queue-null
check.

For each linked nbuf, it first saves and clears `nbuf->next`, then reserves a
TX descriptor. On successful reservation it:

1. Increments queue pending and a local batch count.
2. Writes the physical nbuf data address plus nbuf template words into descriptor
   offsets `+0x0`, `+0x4`, `+0x8`, and `+0x18`.
3. Replaces descriptor `+0x4` bits 1..14 with the modulo-`0x4000` sum of its
   existing encoded length and nbuf length, preserving bits 0 and 15.
4. Stores `nbuf | 1` in the matching owner-ring slot.

It calls IDM ops `+0x70(queue->hardware_queue, count)` after every 256 successful
nbufs and once for a final nonempty partial batch. No barrier is emitted in this
function. A descriptor-allocation failure calls `cpu_net_free_nbuf`, accounts a
device drop when available, logs rate-limited, and continues with the remaining
list; it returns zero unless the queue selector was invalid.

## Accounting Edge

Byte accounting is performed for every successfully reserved nbuf. Packet
accounting is charged at each batch flush to the current nbuf's device. The final
flush uses the last list nbuf even if that item failed descriptor reservation, so
mixed-device lists or a final failure can charge packet counts to a different
device than earlier queued nbufs. Preserve this binary behavior.

## Caller Context

IDA has no direct in-module xrefs. The function is exported as
`__ksymtab_cpu_net_nb_tx`; runtime symbol data and `nm -u` show `ipsec.ko`
imports it. Its exported caller-side locking and list-production contract remain
outside `plat_132`.

The incoming `x1` register is overwritten before any read, so the semantic
signature has one input despite Hex-Rays presenting two arguments.

## Concurrency and Ownership

- No local lock, queue-null check, or memory barrier.
- Successful nbufs become tagged owner-ring entries and are reclaimed by TX
  completion handling.
- Descriptor failures pass nbufs to the gated `cpu_net_free_nbuf` release path.

## Evidence

- Complete 198-instruction ARM64 body at `0xcf14` through `0xd230`.
- No direct in-module xrefs; runtime kallsyms export and `ipsec.ko` undefined
  symbol evidence establish the cross-module boundary.
- Queue reserve/reclaim behavior cross-checked with `net_get_next_txdesc` and
  `net_check_tx_done_nolock`; nbuf release with `cpu_net_free_nbuf`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Exact nbuf producer, selector-map lifecycle, descriptor template words, and
  `ipsec.ko` locking/caller contract.
