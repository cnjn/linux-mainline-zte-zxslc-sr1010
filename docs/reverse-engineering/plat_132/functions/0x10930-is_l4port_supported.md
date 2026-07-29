# 0x10930 is_l4port_supported

## Status

- Status: complete
- Confidence: verified list selection/traversal, port comparison, lock/release
  sequence, return behavior, and both callers.
- Size: `0x9c` bytes, 40 ARM64 instructions.
- Recovered signature:
  `int is_l4port_supported(u16 port, u8 destination)`.

## Semantics

The function selects `supported_dest_ports` when the low byte of `destination`
is nonzero; otherwise it selects `supported_source_ports`. It locks the shared
GRO port-list context with the specialized BH-disabling lock helper, walks a
sentinel-terminated linked list, and returns one when an entry's 16-bit port at
entry offset zero matches the requested port.

The list node begins at entry offset `+0x8`. The sentinel list-head address is
compared directly against the current node, so an empty list returns zero.

After the lookup, the binary releases `groport_busy_lock` with a byte-width
store-release, then calls `__local_bh_enable_ip` with its saved caller return
address and offset `512`. It performs that release/enable sequence for both
found and not-found cases.

## Caller Context

`pp_net_tcp_gro @ 0x10eac` calls this function first for the destination port
and then for the source port when the destination is not SMB port 445. There
are no other in-module callers.

## Concurrency and Ownership

- The port lists are protected by the specialized lock/BH-disable region.
- No allocation, mutation, callback, or ownership transfer occurs here.
- The lock object itself is global and not embedded in either selected list.

## Evidence

- Complete 40-instruction ARM64 disassembly at `0x10930` through `0x109cc`.
- Two direct call xrefs from `pp_net_tcp_gro` at `0x1107c` and `0x1109c`.
- Raw source/destination list-address selection and node/entry arithmetic.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- The original lock type/name and port-entry fields after the node pointer.
- Registration, removal, and lifetime rules for the two configured port lists.
