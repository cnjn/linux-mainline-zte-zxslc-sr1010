# 0x0f258 net_tcp_gso_tx

## Status

- Status: complete
- Confidence: verified validation, linear/fragment payload sourcing, per-segment
  packet construction, checksum/descriptor branches, ownership, return paths,
  caller, and direct helper contracts; raw field labels are strong inference.
- Size: `0x624` bytes, 42 basic blocks.
- Recovered signature:
  `int net_tcp_gso_tx(struct sk_buff *skb, struct net_device *device, u32 direction)`.

## Semantics

This is the ordinary non-upload software TCP GSO sender. The netdev argument is
not read. It derives TCP/shared-info/data pointers from raw skb offsets, clears
PSH bit 3 in the original TCP header before validation, then derives:

```c
header_length = (tcp - skb_data) + 4 * tcp[12].doff;
payload = skb_len - header_length;
linear = skb_len - skb_data_len;
```

If `header_length > linear`, it logs `gso frag_size`/`hlen` and returns `-3`.
PSH remains cleared in the original skb on that failure path. A zero shared-info
GSO size is changed in place to `header_length + 0x57e` (1406).

Payload sourcing starts from linear data when it extends beyond the header;
otherwise it begins at shared-info fragment zero. Fragments use 16-byte records
at `shared_info + 0x30 + 0x10 * index`, with raw page-link/length/offset fields.
Each output segment is capped by the GSO size, allocates an nbuf, copies the
full header, copies payload across linear/fragments, rewrites IPv4 ID/total
length and TCP sequence, and restores PSH only on the final normal segment.

If fragments are exhausted mid-segment, it logs `gso tx bug!!frag`, forces the
remaining source and total payload to zero, then submits the current declared-
length nbuf and returns normal success. This can transmit a truncated final
segment. Later allocation, descriptor, or submit failure can likewise leave
earlier segments transmitted.

## Checksum and Descriptor Branches

With `net_hw_checksum != 0`, it computes IPv4 checksum, clears TCP checksum,
clear, and low-seven-bit IP/TCP offsets encoded in bytes `+8/+9` bits `1..7`.

Without hardware checksum, it invokes `net_gso_checksum_upload(ipv4, tcp, 0)`,

No TX descriptor frees the current nbuf through the bit-gated nbuf free helper,

## Caller Context

`net_gso_tx @ 0x0f87c` is the sole direct caller. It selects this function for
non-upload GSO state while holding the relevant TX lock, examines only the sign
of its result, then always returns zero to its own caller. Its parent TX paths
free the original skb after `net_gso_tx`; submitted nbufs remain queue-owned.

## Concurrency and Ownership

- No local lock. Caller TX lock serializes descriptor ownership; completion uses
  the same queue/lock domain.
- Successful `cpu_net_nb_desc_tx` stores tagged nbuf ownership in the CPU TX
  owner ring. Completion uses `cpu_net_free_nbuf`.
- Global counters and `net_smb_state` writes are non-atomic. Normal exits,
  including zero payload and fragment exhaustion, increment success count and
  set `net_smb_state` to 2.

## Evidence

- Complete 42-block, `0x624`-byte ARM64 function at `0xf258`.
- Sole caller xref from `net_gso_tx @ 0xf87c`.
- Direct disassembly/decompilation plus child analysis for descriptor reserve,
  descriptor configuration, submit ownership, nbuf free, and checksum paths.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Raw skb shared-info/fragment page-link ABI and exact direct-map encoding.
- Descriptor bit names, stats layout, and `net_smb_state` purpose.
- Whether external policy prevents source-fragment exhaustion/truncated sends.
