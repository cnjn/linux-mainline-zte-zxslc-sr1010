# 0x103dc idm_skb_stack_wifi_push

## Status

- Status: complete
- Confidence: verified priority-ordered flag dispatch, selector values, fallback
  release, residual return behavior, and sole direct caller.
- Size: `0x38` bytes, 14 ARM64 instructions.
- Recovered signature: `void idm_skb_stack_wifi_push(struct sk_buff *skb)`.

## Semantics

Reads skb word `+0x114` and applies this priority order:

| Predicate | Action |
| --- | --- |
| bit 17 set | `_idm_skb_stack_push(skb, 0)` |
| bit 17 clear, bit 18 set | `_idm_skb_stack_push(skb, 1)` |
| neither set | `__dev_kfree_skb_any(skb, 1)` |

The function does not modify the skb or validate it locally. Callee return
register values propagate at the machine level, but the sole caller ignores the
result; semantic return type is void.

## Caller Context

`net_check_tx_done_nolock @ 0xb4fc` is the sole direct caller. It invokes this
helper only for untagged completion owners on the IDM TX queue, making this the
Wi-Fi/IDM skb completion-reclamation selection point.

## Globals and Concurrency

No direct global/MMIO access, lock, allocation, or callback. Ownership transfers
to `_idm_skb_stack_push` or to the fixed-reason skb free helper.

## Evidence

- Complete 14-instruction ARM64 body at `0x103dc` through `0x10410`.
- Exact `TBZ` priority order on skb bits 17 and 18, selector values zero/one,
  and fallback reason one.
- Sole direct caller xref in TX completion reclamation.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Semantics/lifecycle of skb word `+0x114` bits 17 and 18.
- External consumer/ownership policy inside `_idm_skb_stack_push`.
