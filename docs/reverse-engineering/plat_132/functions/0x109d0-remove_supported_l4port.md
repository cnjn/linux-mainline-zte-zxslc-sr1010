# 0x109d0 remove_supported_l4port

## Status

- Status: complete
- Confidence: verified selection/traversal, unlink/poison/free order, lock
  release, return behavior, and zero direct in-module xrefs.
- Size: `0xcc` bytes, 49 ARM64 instructions.
- Recovered signature: `int remove_supported_l4port(u16 port, u8 destination)`.

## Semantics

The function selects source or destination GRO port list from the low byte of
`destination`, enters the specialized BH lock, and walks its circular doubly
linked list. It removes only the first entry whose 16-bit port at entry offset
zero matches `port`.

For a match, it repairs both neighbor links, writes these exact poison values to

```c
node->next = (void *)0xdead000000000100ULL;
node->prev = (void *)0xdead000000000122ULL;
```

It releases the low lock byte with store-release, restores BH state with offset
512, and always returns 1, including when no matching port exists. It does not
log a missing entry or remove duplicate entries beyond the first match.

## Caller Context

No direct code or data xrefs to the entry exist in the current IDB. Like the
paired add function, it may be an external or indirect configuration API, but
that remains unproven.

## Concurrency and Ownership

- List traversal and unlink execute under the shared specialized lock.
- A matched list-owned entry transfers to `kfree`; no other ownership changes.

## Evidence

- Complete 49-instruction ARM64 disassembly at `0x109d0` through `0x10a98`.
- Raw neighbor-link, poison, free, and release instruction sequence.
- Zero direct entry xrefs; list layout cross-checked with add and lookup.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- External configuration API/lifetime and duplicate port registration policy.
