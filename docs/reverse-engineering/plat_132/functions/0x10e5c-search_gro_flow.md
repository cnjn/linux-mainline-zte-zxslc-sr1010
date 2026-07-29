# 0x10e5c search_gro_flow

## Status

- Status: complete
- Confidence: verified control flow, bucket calculation, list traversal, return
  behavior, and sole caller.
- Size: `0x50` bytes, 20 ARM64 instructions.
- Recovered signature: `int search_gro_flow(u32 hash)`.

## Semantics

The function calculates bucket `(1640531527U * hash) >> 28`, starts at that
bucket's hlist head, and follows nodes at flow allocation offset `+0x10`. It
subtracts `0x10` to reach the flow allocation and compares the supplied hash
against the 32-bit flow value at offset `+0x8`. It returns one for any matching
hash and zero after reaching the null/sentinel path.

It deliberately does not compare ingress port, IPv4 addresses, TCP ports, or
any skb state. `pp_net_tcp_gro` performs an exact tuple scan separately, then
calls this function as its append-branch gate. Thus hash collision behavior must
remain documented at the caller rather than hidden by this helper.

## Evidence

- Complete 20-instruction ARM64 disassembly at `0x10e5c` through `0x10ea8`.
- Sole direct caller: `pp_net_tcp_gro @ 0x10eac` at `0x11168`.
- Matching bucket multiplier and hlist layout cross-checked against the parent
  function's insertion and exact-flow scan.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- None beyond the already documented parent-side hash-collision edge and the
  original flow allocation type name.
