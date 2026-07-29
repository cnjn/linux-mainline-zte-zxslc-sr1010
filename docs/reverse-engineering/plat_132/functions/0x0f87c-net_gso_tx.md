# 0x0f87c net_gso_tx

## Status

- Status: complete
- Confidence: verified routing gates, callback selection, stats/counters,
  return behavior, skb device assignment, and all callers.
- Size: `0x140` bytes, 79 ARM64 instructions.
- Recovered signature:
  `int net_gso_tx(struct sk_buff *skb, struct net_device *device, u32 path)`.

## Semantics

The function increments `net_gso_cnt`, stores `device` at skb `+0x10`, and
always returns zero. It does not free the original skb; its callers do so after
return.

If skb byte `+0xbb` bit 4 is set, it calls one upload backend selected by
`gso_upload_mode`:

```c
gso_upload_mode ? net_tcp_gso_tx_upload(skb, device, path)
                : net_tcp_gso_tx_upload1(skb, device, path);
```

A negative upload status increments device TX drops; nonnegative status returns
success without local TX packet/byte accounting.

Without upload mode, it requires either skb word `+0x114` bit 14 or nonzero
`*(u16 *)(skb->head + skb->end + 4)` to call `net_tcp_gso_tx`. Success increments
the observed device TX packet counter and adds skb `+0xa8` to TX bytes. Failure
increments GSO failure counter and device drops. A missing raw GSO condition
increments a separate non-TCP-GSO counter and device drops.

## Caller Contract

`cpu_net_tx` and `idm_net_tx` hold their selected TX locks, call this function,
then free the original skb unconditionally. Consequently GSO work owns any
generated nbufs/owner tags while this dispatcher records its own errors but
never exposes a failure return to those callers.

## Evidence

- Complete 79-instruction ARM64 disassembly at `0xf87c` through `0xf9b8`.
- Direct callers: `cpu_net_tx` SW/PON GSO paths and `idm_net_tx` GSO path.
- Raw skb field tests and direct callback argument register setup.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Meaning of skb byte `+0xbb` bit 4 and upload-mode policy are unknown.
- Ownership/error behavior within the upload and TCP-GSO child functions remains
  pending their own reconstruction.
