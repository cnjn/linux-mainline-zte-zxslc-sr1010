# 0x0d234 idm_net_tx

## Status

- Status: complete
- Confidence: verified GSO gate, lock split, descriptor return, backend call,
  queue ownership, stats, all return paths, and raw descriptor/SKB fields.
- Size: `0x268` bytes, 153 ARM64 instructions.
- Recovered signature:
  `netdev_tx_t idm_net_tx(struct sk_buff *skb, struct net_device *device)`.

## Semantics

This is the type-3 IDM netdev transmit callback. It has two paths:

1. If `idm_use_cpu_gso` is nonzero and the skb has raw GSO condition
   (`skb + 0x114` bit 14, `*(u16 *)(head + end + 4)`, or `skb + 0xac`), it locks
   `net_lock_tx`, calls `net_gso_tx(skb, device, 1)`, frees the original skb,
   unlocks, and returns zero. The `net_gso_tx` status is ignored.
2. Otherwise it locks `idm_lock_tx`, obtains a descriptor from `idm_tq`, writes
   descriptor word `+0x14 = 0x04000000`, and invokes `cpu_net_ops + 0x80`
   (`idm_wifi_tx(skb, descriptor)`).

On successful backend submission, it records the skb in the owner slot selected
by `(descriptor - queue_base) >> 5`, increments queue pending count, and updates
observed device TX packets/bytes. A nonzero backend result rolls the producer
index back, increments drops, frees the skb, unlocks, and returns zero.

If no descriptor is available, it frees the skb, increments device drops,
unlocks, rate-limits the message `"idm get tx desc failed\n"`, and returns
`-1`. This is the sole nonzero return path.

## Concurrency

The GSO path serializes on `net_lock_tx`; the direct IDM path serializes on
`idm_lock_tx`. Both select raw lock versus irqsave lock from
`*(u32 *)(SP_EL0 + 0x10) & 0x1fff00`, then release the lock's low byte with a
store-release and restore IRQ state when applicable.

## Backend Contract

`idm_wifi_tx @ 0x14be4` writes the physical skb data address and length/port
fields, applies `data_padding` for frames no longer than 59 bytes, issues a DSB
store barrier, rings its hardware doorbell with `0x20000`, and returns zero in
the installed implementation. The caller retains its compiled backend-failure
branch for alternate operations providers.

## Evidence

- Complete 153-instruction ARM64 disassembly at `0xd234` through `0xd498`.
- Type-3 netdev operations-table reference at `0x1dec0`.
- Direct `idm_wifi_tx @ 0x14be4` decompilation.
- Shared TX queue/reclaim analysis in `net_get_next_txdesc` and
  `net_check_tx_done_nolock`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- The rationale for exposing descriptor exhaustion as `-1` here while
  `cpu_net_tx` always reports success requires caller/netdev-core context.
- Exact semantics of skb flag bit 14 and the descriptor `0x04000000` control
  word remain unresolved.
