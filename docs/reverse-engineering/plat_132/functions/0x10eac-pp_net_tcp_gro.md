# 0x10eac pp_net_tcp_gro

## Status

- Status: complete
- Confidence: verified for control flow, flow state layout, major eligibility
  gates, descriptor/SKB ownership, flush behavior, and return values; strong
  inference for raw descriptor/SKB field labels and fragment encoding.
- Size: `0x660` bytes, 406 ARM64 instructions.
- Recovered signature:
  `int pp_net_tcp_gro(void *descriptor, struct net_device *device, const void *data, u32 queue, u32 jumbo)`.

## Semantics

The function is the CPU RX TCP GRO aggregator. It receives a raw IDM descriptor
and its mapped packet data, verifies that descriptor length equals the IPv4
total length plus the descriptor payload offset, then derives the IPv4 and TCP
headers. A preceding network-order value `0x0021` causes it to retain an
additional header pointer at `ipv4 - 8` for the eventual aggregated skb.

It builds a 32-bit Jenkins-style hash from the raw TCP source/destination ports
and IPv4 source/destination addresses. The hash indexes a 16-bucket hlist whose
nodes begin at flow allocation offset `+0x10`; each allocation starts with an
skb pointer at `+0x0` and the hash at `+0x8`. At most 16 flows can exist.

Port 445 bypasses the configurable port allow lists. Other traffic needs either
the destination or source TCP port in its respective supported-port list.
`can_tcp_gro` additionally rejects unsuitable memory, selected TCP control
flags, small initial segments, mismatched ingress/IP/port tuples, and a
specific raw sequence/ACK formula documented in its own function record.

When eligibility fails, this function flushes, unlinks, decrements, and frees
every pending flow before returning zero. The caller then takes its normal skb
RX path. On an eligible flow continuation, it adds one shared-info fragment,
stores the raw descriptor buffer value in the skb's vendor buffer-pointer array,
and increases raw skb length/data-length/truesize fields. It flushes and removes
count reaches `max_gro`.

A new flow allocates a slab object and an skb attached to the existing normal
buffer. It inserts the flow into the selected hlist, snapshots the first 32-byte
descriptor in global Wi-Fi trap state, sets raw skb bookkeeping/metadata fields,
records IPv4/TCP pointers, and returns one. Successful return means CPU RX does
not clear descriptor word zero in its caller; the aggregated skb retains the
raw buffer values until flush/delivery.

## Notable Edge Behavior

The binary first finds an exact tuple match, but its append branch is selected
by `search_gro_flow(hash, exact_flow)`, whose machine code tests only whether a
flow with the same hash exists. The branch then dereferences the exact-flow
pointer without a null check. This is harmless for ordinary unique hashes but
has no observed collision defense; the recovery preserves that behavior rather
than silently adding one.

## Caller and Callees

- Sole caller: `cpu_net_rx @ 0x0c5dc`, after successful normal-buffer refill,
  when descriptor byte 14 matches the GRO mask, descriptor byte 8 is not
  `0xfd`, pool is normal, `switch_skb_recv` is installed, and `net_gro_en` is
  nonzero.
- Direct helpers: `is_l4port_supported @ 0x10930`, `can_tcp_gro @ 0x10a9c`,
  `search_gro_flow @ 0x10e5c`, `pp_tcp_gro_flush @ 0x10c24`,
  `hlist_del_init`, `kmem_cache_alloc`, `alloc_skb_attach_buffer`, `skb_put`,
  `cpu_dev_stat`, and `kfree`.
- `pp_tcp_gro_flush_all @ 0x10dcc` performs the same 16-bucket flush/unlink/free
  sweep and is used by the surrounding non-GRO CPU RX/poll paths.

## Concurrency and Ownership

- No local lock protects the hash table, flow count, counters, or global Wi-Fi
  descriptor snapshot. The design relies on the CPU RX/NAPI execution context.
- A created flow takes ownership of the current packet buffer through its
  attached skb. A continuation retains the next raw descriptor buffer in the
  skb's vendor buffer-pointer array and adds its TCP payload as a fragment.
- Flushing transfers the aggregate skb to `idm_skb_recv` for qualifying ports
  when installed, otherwise to `switch_skb_recv`; that behavior is in
  `pp_tcp_gro_flush`.

## Evidence

- Complete 406-instruction ARM64 disassembly at `0x10eac` through `0x11508`.
- Direct decompilation and register-level checks of the exact-flow lookup,
  flow creation, fragment append, failure sweep, and threshold flush paths.
- Direct decompilation of `can_tcp_gro @ 0x10a9c`,
  `search_gro_flow @ 0x10e5c`, `pp_tcp_gro_flush @ 0x10c24`, and
  `pp_tcp_gro_flush_all @ 0x10dcc`.
- Sole caller xref from `cpu_net_rx @ 0xc5dc` and its recovered descriptor
  ownership branch.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Exact vendor names and semantics of descriptor byte 13/14, skb offsets
  `0x10e`, `0x108`, `0x114`, `0x120`, `0x128`, and `0x138`, and the encoded
  fragment page-link value.
- The reserved-memory comparison and TCP control-flag mask in `can_tcp_gro`.
- Whether Wi-Fi global descriptor state is intentionally single-flow scoped or
  relies on a stronger serialized execution guarantee than visible here.
