# 0x0afd8 cpu_net_alloc_nbuf

## Status

- Status: complete
- Confidence: verified.
- Size: `0x20` bytes, 8 ARM64 instructions.
- Recovered signature: `void *cpu_net_alloc_nbuf(void)`.

## Semantics

The wrapper loads `cpu_net_ops`, calls its function pointer at offset `+0x28`
with no explicit argument, and returns that pointer unchanged. It does not
validate `cpu_net_ops` or the callback result.

## Caller Context

Direct callers are `gso_upload_enable`, `net_tcp_gso_tx_upload`,
`net_tcp_gso_tx_upload1`, and `net_tcp_gso_tx`.

## Concurrency and Ownership

- No local lock or allocation policy exists in the wrapper.
- Ownership and failure contract belong to the IDM operations-table callback.

## Evidence

- Complete eight-instruction ARM64 body at `0xafd8` through `0xaff4`.
- Four direct callers and confirmed IDM ops-table offset `+0x28`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Original allocated nbuf type and callback ownership contract.
