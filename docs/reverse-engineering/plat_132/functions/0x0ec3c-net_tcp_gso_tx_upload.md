# 0x0ec3c net_tcp_gso_tx_upload

## Status

- Status: complete
- Confidence: verified header/pool/length/checksum/submit/failure flow and sole
  caller; raw skb/nbuf field labels and outer-protocol marker are strong
  inference.
- Size: `0x2fc` bytes.
- Recovered signature:
  `int net_tcp_gso_tx_upload(struct sk_buff *skb, struct net_device *device, u32 path)`.

## Semantics

This is the upload-mode GSO segment template builder. It derives a GSO segment
size from skb shared info; zero uses `1514 - header_length`. It calculates a
network-header offset, TCP header length, total header length, and TCP payload
length, increments an upload-attempt counter, and uses the first segment payload
as `min(payload_length, gso_segment_size)`.

It optionally rewrites a 16-bit field just before the network header when a
raw pre-header marker equals `0x6488`; the exact protocol label remains
unresolved. It then chooses an nbuf from the 64-slot pool if published, advancing
`gso_buf_idx` with low-byte modulo-64 arithmetic, or allocates a fresh nbuf when
no pool is published.

For a pool entry, if the current header is shorter than that slot's last header,
null. Thus a null selected pool slot can be dereferenced before the later
failure return when its saved header length is larger. It always writes the
slot's new last-header length before that null test.

After nbuf selection, it zeroes a payload-sized area as applicable, copies only
the computed header from skb data, sets nbuf length at `+0x28`, and replaces
nbuf byte `+0x2c` bit 0 with `path & 1`.

- IPv4: updates total length at `+2`, computes upload checksum, increments an
  IPv4 counter.
- IPv6: updates payload length at `+4`, computes IPv6 TCP checksum unless next
  header byte `+6` is 4. For that nested case it rewrites outer/inner length
  fields, clears inner IPv4 checksum, and uses the IPv4 checksum helper.
- Other versions: submit without header checksum rewrite.

It finally calls `net_gso_upload_send(nbuf, skb, payload_length, gso_size)` and
returns its status. Nbuf acquisition failure increments a failure counter and
returns `-1`; it does not free an nbuf on that path.

## Caller Context

The sole direct caller is `net_gso_tx @ 0x0f87c` when skb byte `+0xbb` bit 4 is
set and `gso_upload_mode` is nonzero. The passed netdev argument is not read in
this function.

## Concurrency and Ownership

- No local lock; `net_gso_tx` runs under the selected caller TX lock.
- Pool nbuf reuse and ownership after submit are delegated to
  `net_gso_upload_send`/`cpu_net_nb_desc_tx`.
- The function does not copy ordinary TCP payload into the observed nbuf range;
  payload transfer semantics are delegated to descriptor/configuration helpers.

## Evidence

- Complete `0x2fc`-byte ARM64 function at `0x0ec3c`.
- Sole caller argument setup in `net_gso_tx`.
- Direct raw field, pool, checksum helper, and descriptor-submit calls.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Exact raw skb/nbuf offsets and the pre-header `0x6488` marker protocol.
- How original payload is associated with the descriptor after only header copy.
- Pool-slot null scenario and whether external state guarantees it cannot occur.
