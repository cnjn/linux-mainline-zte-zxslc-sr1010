# 0x0b4b0 cpu_net_free_nbuf

## Status

- Status: complete
- Confidence: verified bit gate, raw pointer arithmetic, ops callback, counter,
  and five callers; nbuf field labels are strong inference.
- Size: `0x4c` bytes, 19 ARM64 instructions.
- Recovered signature: `void cpu_net_free_nbuf(void *nbuf)`.

## Semantics

The wrapper checks nbuf byte `+0x2c` bit 1. When clear, it loads the pointer at
nbuf `+0x10`, subtracts 64 bytes, and calls `cpu_net_ops->free_buffer(buffer, 0)`

When bit 1 is set, it intentionally does not call the backend free callback and
instead increments `g_nb_not_rls_cnt`. Semantic return type is void; the
decompiler's apparent pointer return is residual register state.

## Caller Context

Direct callers are `net_check_tx_done_nolock`, `cpu_net_nb_tx`,
`net_gso_upload_send`, `gso_upload_disable`, and `net_tcp_gso_tx`.
`gso_upload_disable(1)` explicitly clears bit 1 before calling this function,
which enables actual backend release.

## Concurrency and Ownership

- No local lock or allocation occurs in the wrapper.
- Bit 1 is an observed ownership/release gate; setting it retains the nbuf and
only accounts a non-release event.

## Evidence

- Complete 19-instruction ARM64 body at `0xb4b0` through `0xb4f8`.
- Five direct caller xrefs and ops-table mapping at `+0x30`.
- Exact bit test, pointer subtract, and counter increment instructions.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Original nbuf type/field names and semantics of the non-release counter.
