# 0x10864 add_supported_l4port

## Status

- Status: complete
- Confidence: verified allocation/initialization, list insertion, lock/release,
  return behavior, and zero direct in-module xrefs; cache/list labels are
  strong inference.
- Size: `0xcc` bytes, 48 ARM64 instructions.
- Recovered signature: `int add_supported_l4port(u16 port, u8 destination)`.

## Semantics

The function allocates one 24-byte GRO state-cache entry with allocation flags
`0xcc0`. Allocation failure logs an unresolved static message and returns `-1`.
On success it zeroes all 24 bytes, writes the supplied 16-bit port at offset
zero, then acquires the specialized GRO port-list lock.

A nonzero low byte of `destination` selects `supported_dest_ports`; zero selects
`supported_source_ports`. Each list is a circular doubly linked list with a
16-byte sentinel head. The entry has a node at `+0x8`; it is appended at the
tail in the observed write order:

```c
tail = list->head.prev;
list->head.prev = &entry->node;
entry->node.next = &list->head;
entry->node.prev = tail;
tail->next = &entry->node;
```

It releases the low byte of `groport_busy_lock` with store-release, restores BH
state with offset 512, and returns 1. It does not deduplicate ports.

## Caller Context

There are no direct code/data xrefs to the entry in the current IDB. The
function is likely an external or indirect configuration API, but that is not
proved by its lack of direct xrefs.

## Concurrency and Ownership

- Successful entries are linked into one port list and no local removal/free is
  performed here.
- List mutation is serialized by the specialized BH lock and paired low-byte
  release used by lookup and remove paths.

## Evidence

- Complete 48-instruction ARM64 disassembly at `0x10864` through `0x1092c`.
- Direct raw list-head/node offset writes and shared lock helper call.
- Zero direct entry xrefs; source/destination globals cross-checked with lookup.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Static allocation-failure string contents and the cache's original type/name.
- External registration API and lifetime/duplicate-port policy.
