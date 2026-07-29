# 0x0df4c net_cfg_desc_by_skb

## Status

- Status: complete
- Confidence: verified direct writes, port branch, return use, and all callers;
  descriptor field labels are strong inference.
- Size: `0x5c` bytes, 23 ARM64 instructions.
- Recovered signature:
  `void net_cfg_desc_by_skb(void *descriptor, struct sk_buff *skb, u32 direction)`.

## Semantics

The helper writes descriptor raw state:

```c
u32[desc + 0x18] = 0;
u32[desc + 0x04] = 0;
u32[desc + 0x08] = 0x00400000;
desc[7] = 15;
desc[27] = (desc[27] & 0xf3) | 8;
```

If `direction != 0` or `lan_up != 0`, it reads skb byte `+0x108`. When the
wrapped value `(u8)(port - 16)` is above `0x20`, it increments the port. It then
replaces descriptor byte `+0x0a` low six bits with that port, preserving high
two bits. The apparent return of the descriptor pointer is residual register
state; all callers ignore it, so the semantic signature is void.

## Caller Context

`net_gso_upload_send` calls it once per upload descriptor. `net_tcp_gso_tx`

## Concurrency and Ownership

No lock, allocation, MMIO, callback, or ownership operation. It only mutates

## Evidence

- Complete 23-instruction ARM64 body at `0xdf4c` through `0xdfa4`.
- Three direct caller xrefs and raw descriptor/skb argument setup.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Original names/meaning of raw descriptor constants and port mapping rule.
