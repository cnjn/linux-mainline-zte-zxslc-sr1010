# 0x0e7f4 net_gso_checksum_upload

## Status

- Status: complete
- Confidence: verified checksum inputs, upload-template branch, TCP/IP writes,
  return use, and callers; raw header labels are strong inference.
- Size: `0xa4` bytes.
- Recovered signature:
  `void net_gso_checksum_upload(void *ipv4_header, void *tcp_header, u32 upload_template)`.

## Semantics

The helper zeroes TCP checksum at header `+16`, derives full TCP length as:

```c
be16(ipv4 + 2) - 4 * (ipv4[0] & 0x0f)
```

For `upload_template == 0`, it runs `csum_partial` over that full TCP length.
For nonzero `upload_template`, it runs `csum_partial` only over
`4 * (tcp[12] >> 4)` bytes, while still using the full TCP length in
`csum_tcpudp_nofold`. It folds/complements that result into TCP checksum and
then calls `ip_send_check` on the IPv4 header.

The decompiler residual return is the imported `ip_send_check` result; callers
ignore it, so the semantic signature is void.

## Caller Context

- `net_tcp_gso_tx @ 0x0f258` uses mode 0 for ordinary full software segments.
- `net_tcp_gso_tx_upload @ 0x0ec3c` and
  `net_tcp_gso_tx_upload1 @ 0x0ef38` use mode 1 for upload templates/segments.

## Concurrency and Ownership

No global state, allocation, locking, MMIO, callback, or ownership transfer.
It mutates only supplied IPv4/TCP header buffers.

## Evidence

- Direct decompilation and complete function body at `0xe7f4` through `0xe898`.
- Exact `csum_partial`, `csum_tcpudp_nofold`, and `ip_send_check` argument setup.
- Three GSO sender call contexts.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Why upload-template mode checksums only TCP header bytes before full-length
  pseudo-header fold; downstream payload/checksum offload contract is external.
