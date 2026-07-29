# 0x10758 hlist_del_init

## Status

- Status: complete
- Confidence: verified complete two-pointer update, no-op condition, node reset,
  all three GRO callers, and semantic return type.
- Size: `0x20` bytes, 8 ARM64 instructions.
- Recovered signature: `void hlist_del_init(zte_gro_hlist_node_t *node)`.

## Semantics

Treats the input as a two-pointer hlist node: `next` at offset zero and `pprev`
Otherwise it replaces `*pprev` with `next`, updates `next->pprev` when a successor
exists, and clears both node pointers.

The residual X0 register is the original node pointer, but no caller uses it;
the recovered interface is semantic `void`. There is no validation, locking, or
memory barrier, so callers own list serialization and node lifetime.

## Caller Context

Three direct GRO cleanup call sites were found:

- `pp_tcp_gro_flush_all @ 0x10dcc` unlinks every pending aggregate flow.
- `pp_net_tcp_gro @ 0x10eac` unlinks an evicted flow and a completed aggregate.

Each caller frees or otherwise disposes of the enclosing flow after unlinking.

## Evidence

- Complete ARM64 body at `0x10758` through `0x10774`.
- `CBZ X2` at `0x1075c` proves a null-`pprev` no-op.
- Stores at `0x10764`, `0x1076c`, and `0x10770` establish successor repair and
  zeroing of both fields.
- Three direct xrefs at `0x10e14`, `0x11104`, and `0x114bc`.
- Existing GRO flow layout in `recovered/plat_cpu_net.c` independently matches
  the two-pointer node representation.

## Source-Like Reconstruction

`recovered/plat_cpu_net.c`.
