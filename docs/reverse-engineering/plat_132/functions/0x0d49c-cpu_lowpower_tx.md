# 0x0d49c cpu_lowpower_tx

## Status

- Status: complete
- Confidence: verified callback gates/call arguments, device/work-mode branches,
  repeated judge behavior, descriptor mutation, return use, and both callers;
  callback purpose and descriptor field names are strong inference.
- Size: `0x110` bytes, 68 ARM64 instructions.
- Recovered signature:
  `void cpu_lowpower_tx(zte_net_device_t *device, zte_skb_t *skb, zte_tx_descriptor_t *descriptor)`.

## Semantics

The helper exits unless both externally registered callback slots are non-null
and the first `low_power_up_en_judge()` call is nonzero. It skips device type 3.
For a non-management device, PON work mode with no `0xe40` bits and at least one
`0x1a0` bit, it calls:

```c
low_power_send(0, 0, skb_data_at_0x130, skb_length_at_0xa8, 0);
```

It then calls the judge once or twice more. The exact repeated results, together
with repeated device type `+0x888` loads, decide whether to modify a descriptor:
when the low seven bits of descriptor byte `+0x0a` equal 64, descriptor `+0x4`
bits 1..14 are replaced with `(skb_length + 4) & 0x3fff`, preserving bits 0 and
15. It has no return contract; the machine's residual `x0` value is ignored.

## Callback Boundary

- `regisetr_low_power_send_pkt_handle @ 0x0afc0` stores a five-argument callback
  in `low_power_send`.
- `regisetr_low_power_up_en_judge_handle @ 0x0afcc` stores a no-argument callback
  in `low_power_up_en_judge`.
- Neither registration helper has a direct in-module caller, so callback lifetime
  and side effects remain an external module contract.

## Caller Context

`cpu_net_tx @ 0x0d668` calls this after descriptor setup for SW and PON direct
TX, before the CPU backend submit callback. Both paths hold `net_lock_tx`.

## Concurrency and Ownership

- No local lock, allocation, free, owner-ring update, or barrier.
- It directly invokes mutable callback slots after an initial null test; it does
  not stabilize callback pointers across calls.
- It mutates only the supplied descriptor's encoded length field.

## Evidence

- Complete 68-instruction ARM64 body at `0xd49c` through `0xd5a8`.
- Two direct caller xrefs in SW/PON `cpu_net_tx` paths.
- Callback storage functions at `0xafc0` and `0xafcc` establish the observed ABI.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Meaning of judge return value 2, descriptor port value 64, mode gating, and
  the side effects/ownership expectations of `low_power_send`.
