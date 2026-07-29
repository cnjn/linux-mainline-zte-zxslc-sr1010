# 0x0e964 gso_upload_enable

## Status

- Status: complete
- Confidence: verified gate/allocation/initialization/failure cleanup/global
  publication behavior and sole caller; nbuf field labels are strong inference.
- Size: `0xdc` bytes, 53 ARM64 instructions.
- Recovered signature: `void gso_upload_enable(void)`.

## Semantics

If `gso_buf_cnt` is nonzero, this function returns without mutation. Otherwise
it allocates 64 nbufs through `cpu_net_alloc_nbuf`. For each successful nbuf it
sets byte `+0x2c` bit 1, zeroes the buffer pointed to by nbuf `+0x18` for:

```c
uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE - uBP_BUFFER_OFFSET - 64
```

then stores the nbuf in `gso_nbuf_pool[index]` and clears
`s_gso_last_hlen[index]`.

Only after all 64 allocations succeed does it set `gso_buf_cnt = 64` and
`gso_buf_idx = 0`. On the first allocation failure, it logs a ratelimited error
and calls `gso_upload_disable(1)`. That helper iterates only while
`index < gso_buf_cnt`; because this function has not yet published a nonzero
count, the call frees none of the already stored partial nbufs. The partial pool
therefore leaks in the observed binary. The call's return value is not
semantically used.

## Caller Context

The sole direct caller is `net_upload_fun @ 0x0ea40`, which holds `net_lock_tx`
and invokes it only before incrementing an upload reference from zero.

## Concurrency and Ownership

- No local lock; caller serialization is required.
- Successful nbufs become pool-owned. Before count publication, failure leaves
  already allocated/stored nbufs unreachable through the cleanup loop.

## Evidence

- Complete 53-instruction ARM64 disassembly at `0xe964` through `0xea3c`.
- Sole caller xref and direct GSO global/nbuf writes.
- Failure branch setup for `gso_upload_disable(1)`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Original nbuf type and exact meanings of byte `+0x2c` and pointer `+0x18`.
- Exact nbuf ABI/field names remain open; the paired disable loop confirms the
  partial-allocation leak.
