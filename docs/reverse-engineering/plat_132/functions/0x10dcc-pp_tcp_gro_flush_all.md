# 0x10dcc pp_tcp_gro_flush_all

## Status

- Status: complete
- Confidence: verified complete loop/control flow, flow cleanup order, return
  use, and both callers.
- Size: `0x90` bytes.
- Recovered signature: `void pp_tcp_gro_flush_all(void)`.

## Semantics

The function iterates all 16 `gro_hash_table` buckets. For every hlist node it
saves `node->next`, converts the node at allocation offset `+0x10` back to its
flow allocation, calls `pp_tcp_gro_flush(flow->skb)`, unlinks the node, decrements
`g_cur_flows`, frees the flow allocation, and proceeds to the saved next node.

Its decompiler residual return is literal zero; all callers ignore it, so the
semantic source-like signature is void.

## Caller Context

- `cpu_net_rx @ 0x0c5dc` invokes it before the ordinary skb attachment/delivery
  path after a packet does not remain in GRO.
- `cpu_net_poll @ 0x0cce4` invokes it after its queue scan before conditionally
  completing and unmasking NAPI source zero.

## Concurrency and Ownership

- No local lock protects table traversal, `g_cur_flows`, or callback delivery.
- Each pending aggregate skb transfers to `pp_tcp_gro_flush` receiver ownership;
  this function frees only the enclosing GRO flow allocation after that call.

## Evidence

- Direct decompilation of the complete `0x90`-byte body.
- Two direct caller xrefs from CPU RX and CPU poll.
- Cross-check with the identical inlined sweep in the failed-eligibility branch
  of `pp_net_tcp_gro`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- No new questions beyond GRO hash-table synchronization and receiver callback
  ownership already recorded at the component boundaries.
