# 0x10c24 pp_tcp_gro_flush

## Status

- Status: complete
- Confidence: verified delivery branching, raw skb/header mutations, callback
  arguments, accounting, return use, and callers; field labels are strong
  inference.
- Size: `0x1a8` bytes, 106 ARM64 instructions.
- Recovered signature: `void pp_tcp_gro_flush(struct sk_buff *skb)`.

## Semantics

The function delivers one pending GRO aggregate skb. It derives vendor
shared-info state from raw skb offsets `+0x120` and `+0x128`. If the shared-info
byte at `+0x2` is nonzero, it adds skb word `+0xac` to the network-order IPv4
total length at the saved IPv4 pointer (`skb + 0x30`), and does the same at
offset `+4` of the optional saved outer header (`skb + 0x40`). It then calls
`ip_send_check` on the inner IPv4 header.

For skb byte `+0x108` in the inclusive raw range 16 through 48, a non-null
`idm_skb_recv` selects the Wi-Fi trap path: it overwrites skb device at `+0x10`
with the global Wi-Fi GRO netdev, zeroes the 36-byte trap record from the global
32-byte descriptor snapshot and saved RX queue before calling `idm_skb_recv`.
Every other case calls `switch_skb_recv` with no null check.

After delivery it increments the GRO flush counter, adds `shared_info[2] + 1`
to a segment counter, changes `net_smb_state` to 1 only when it was not already
1, and increments a second raw completion counter. The apparent return register
is stack-canary residue; all callers ignore it, so semantic return type is void.

## Caller and Ownership Context

- `pp_net_tcp_gro @ 0x10eac` calls it when candidate eligibility fails for all
  flows and when an append reaches the short-payload/fragment limit.
- `pp_tcp_gro_flush_all @ 0x10dcc` calls it while sweeping every pending bucket.
- The receiving callback owns the aggregate skb after this function invokes it.
  This function itself does not free the skb or flow allocation.

## Concurrency

There is no local lock. The global Wi-Fi descriptor snapshot and queue are read
as-is, so correctness depends on the serialized GRO/NAPI execution context.

## Evidence

- Complete 106-instruction ARM64 disassembly at `0x10c24` through `0x10dc8`.
- Three direct caller xrefs: two in `pp_net_tcp_gro` and one in
  `pp_tcp_gro_flush_all`.
- Direct callback, checksum, global-counter, and raw offset argument setup.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Original names of skb offsets `0x108`, `0x120`, `0x128`, and `0xac`.
- Ownership/lifetime contract of `wifi_gro_netdev` and receiver callbacks.
- Meaning of `net_smb_state` and the two raw delivery counters.
