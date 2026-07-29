# 0x0d5ac cpu_net_pon_set_desc

## Status

- Status: complete
- Confidence: verified descriptor writes, work-mode gate, QoS callback use,
  skb metadata state machine, and sole caller. Semantic return type is void.
- Size: `0xbc` bytes, 47 ARM64 instructions.
- Recovered signature:
  `void cpu_net_pon_set_desc(struct sk_buff *skb, void *descriptor)`.

## Semantics

The function unconditionally writes:

```text
descriptor + 0x18 = 0x08000000
descriptor + 0x10 = 0
descriptor + 0x14 = 0
```

When work-mode bit `0x10` is clear, it sets skb byte `+0x108` to zero and
returns. When the bit is set, an installed `dev_qos_select_queue(skb)` callback
replaces the low 9 bits of descriptor halfword `+0x1a`; without that callback,
it clears those nine bits directly.

It then updates skb byte `+0x108`:

| Condition | Value |
| --- | --- |
| `pon_up_flag != 1` | prior byte plus one, modulo 256 |
| `pon_up_flag == 1 && lan_up == 1` | low byte of `lan_up_port` |
| `pon_up_flag == 1 && lan_up != 1` | zero |

IDA shows residual `x0` values on return, but the only caller ignores them;
the semantic function return type is `void`.

## Evidence

- Complete 47-instruction ARM64 disassembly at `0xd5ac` through `0xd664`.
- Sole direct caller `cpu_net_tx @ 0xd668` at `0xd9d8`.
- Callback call setup preserves `skb` in `x0`, establishing
  `dev_qos_select_queue(skb)` rather than a zero-argument callback.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Exact hardware semantics of descriptor control word `0x08000000` and QoS bits
  are unresolved.
- The upstream meaning of skb byte `+0x108` remains inferred from RX/TX usage.
