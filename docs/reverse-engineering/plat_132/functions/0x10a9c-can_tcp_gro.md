# 0x10a9c can_tcp_gro

## Status

- Status: complete
- Confidence: verified for all predicates, arithmetic, return behavior, and
  sole caller context; raw structure/region labels remain strong inference.
- Size: `0x184` bytes, 98 ARM64 instructions.
- Recovered signature:
  `int can_tcp_gro(struct sk_buff *flow_skb, const u8 *ipv4, const u8 *tcp, const void *descriptor)`.

## Semantics

The function returns zero or one without allocations, callbacks, mutation, or
locking. It is the eligibility gate used only by `pp_net_tcp_gro`.

It first calculates a vendor reserved-memory threshold from `idm_reserved_base`,

It rejects a raw 32-bit TCP word at offset 12 when mask `0x2f00` is nonzero.
For a new flow (`flow_skb == NULL` and `g_cur_flows <= 15`), it accepts only
when:

```c
be16(ipv4 + 2) - 4 * ipv4_ihl - 4 * tcp_doff > 0x54f
```

For an existing flow, it requires matching descriptor port (`descriptor[6] &
0x3f`), IPv4 source/destination words, and the raw TCP source/destination port
word. It then returns true only when both of these exact binary predicates hold:

```c
be32(old_tcp + 4) + skb_u32(flow_skb, 0xac) + current_payload_len ==
    be32(current_tcp + 4)
raw_u32(old_tcp + 8) == raw_u32(current_tcp + 8)
```

The field at skb `+0xac` is known to accumulate alongside skb `+0xa8` during
GRO append, but its original field name is not established. The formula above
is intentionally recorded literally rather than inferred as a conventional TCP
sequence-continuity rule.

## Edge Behavior

If `g_cur_flows > 15` and no exact flow is supplied, the new-flow branch is not
sole caller invokes this function before applying its own `g_cur_flows == 16`
allocation limit. This is an observed unguarded binary edge, not a defensive
reconstruction omission.

## Evidence

- Complete 98-instruction ARM64 disassembly at `0x10a9c` through `0x10c20`.
- Exact argument setup from `pp_net_tcp_gro @ 0x10eac`.
- Direct raw field loads, condition codes, byte-swap calls, and configuration
  global references in the assembly.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Exact reserved-memory threshold semantics and whether its virtual comparison
  is a pool or address-class exclusion.
- TCP flag names represented by raw mask `0x2f00` on this vendor layout.
- Original name and intended sequence meaning of skb field `+0xac`.
