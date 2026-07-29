# 0x0e788 net_gso_ipv6tcp_checksum.constprop.6

## Status

- Status: complete
- Confidence: verified.
- Size: `0x6c` bytes, 27 ARM64 instructions.
- Recovered signature:
  `void net_gso_ipv6tcp_checksum_constprop_6(void *ipv6_header, void *tcp_header)`.

## Semantics

The helper clears TCP checksum at `tcp + 16`, computes a partial checksum over
`4 * (tcp[12] >> 4)` bytes, byte-swaps IPv6 payload length at `ipv6 + 4`, and
stores `csum_ipv6_magic(ipv6 + 8, ipv6 + 24, payload_length, 6, partial)` at
TCP checksum offset 16.

It does not recalculate an IPv6 header checksum because IPv6 has none.

## Caller Context

The two upload segmenters use it only for IPv6 packets whose next-header byte
at `+6` is not 4. IPv6 next-header 4 takes their nested IPv4 checksum path.

## Concurrency and Ownership

No global state, allocation, lock, MMIO, callback, or ownership behavior.
It mutates supplied TCP header only.

## Evidence

- Complete 27-instruction ARM64 body at `0xe788` through `0xe7f0`.
- Two direct caller xrefs from the upload segmenters.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.
