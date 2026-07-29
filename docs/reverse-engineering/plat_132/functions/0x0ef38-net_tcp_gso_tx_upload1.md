# 0x0ef38 net_tcp_gso_tx_upload1

## Status

- Status: complete
- Confidence: verified segmentation loop, header/sequence/checksum rewrites,
  failure behavior, caller, and submit arguments; raw field/marker labels are
  strong inference.
- Size: `0x320` bytes, 199 ARM64 instructions.
- Recovered signature:
  `int net_tcp_gso_tx_upload1(struct sk_buff *skb, struct net_device *device, u32 path)`.

## Semantics

This is the alternate upload-mode software segmenter. It derives GSO size and
raw header offsets from skb state, substitutes `1514 - header_length` when GSO
size is zero, derives the initial TCP sequence in host byte order, and increments
an upload1 attempt counter. The netdev argument is not read.

For a matching raw pre-header marker `0x6488`, it updates a 16-bit outer field
once using the first segment length. It then loops until all TCP payload bytes
are consumed:

1. Allocate a fresh nbuf; failure increments the nbuf-failure counter and
   returns `-1`.
2. Select `min(remaining_payload, gso_size)`, zero that payload range after the
   nbuf header, and copy the skb header only.
3. Store nbuf length, set nbuf byte `+0x2c` bit 0 from `path`, write the current
   TCP sequence, then advance the host-order sequence by segment length.
4. Rewrite IPv4 or IPv6 payload/total-length fields and checksums. IPv6 next
   header 4 takes a nested IPv4 rewrite/checksum path.
5. Submit with `net_gso_upload_send(nbuf, skb, segment_length, segment_length)`.
   A negative submit status stops the loop and returns `-1`.

Zero payload returns 0 without nbuf allocation. Unlike `net_tcp_gso_tx_upload`,
this function does not use the published 64-entry upload pool and creates a new
nbuf per segment.

## Caller Context

The sole direct caller is `net_gso_tx @ 0x0f87c` when skb byte `+0xbb` bit 4 is
set and `gso_upload_mode` is zero.

## Concurrency and Ownership

- No local lock; its caller runs under TX serialization.
- Each successful nbuf transfers to `net_gso_upload_send`/descriptor submission.
- No direct nbuf cleanup follows a negative send result; the send/helper path
  determines whether the nbuf was freed or retained.

## Evidence

- Complete 199-instruction ARM64 body at `0xef38` through `0xf254`.
- Sole caller xref and the direct alternate-mode branch in `net_gso_tx`.
- Exact allocation, sequence increment, checksum, and submit register setup.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Raw skb/nbuf field names, pre-header marker protocol, and external payload
  association after header-only nbuf copy.
- Ownership behavior for negative downstream submit status.
