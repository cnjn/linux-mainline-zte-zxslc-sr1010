# 0x0b1a0 dev_kfree_skb_any

## Status

- Status: complete
- Confidence: verified fixed forwarding call, argument value, imported target,
  all in-module callers, and semantically unused residual return register.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `void dev_kfree_skb_any(zte_skb_t *skb)`.

## Semantics

Thin local wrapper around the imported kernel helper:

```c
__dev_kfree_skb_any(skb, 1);
```

The incoming skb pointer remains in `x0`; the wrapper writes literal `1` to
`w1`, then calls `__dev_kfree_skb_any`. It has no null check, local state,
locking, or ownership branch. The residual `x0` register at `RET` is not used by
any direct caller, so the semantic return type is void.

## Caller Context

There are 13 direct in-module call sites: TX failure/drop paths in
`cpu_net_tx @ 0x0d668` and `idm_net_tx @ 0x0d234`, plus untagged-owner reclaim
in `net_check_tx_done_nolock @ 0x0b4fc`. These callers hand ownership of the skb
to the imported release helper through this wrapper.

## Concurrency and Ownership

- No local synchronization or allocation.
- Ownership passes directly to `__dev_kfree_skb_any`; caller-side lock context
  varies by TX path.

## Evidence

- Complete six-instruction ARM64 body at `0xb1a0` through `0xb1b4`.
- Direct `BL __dev_kfree_skb_any` import at `0xb1ac`; literal second argument is
  `1` at `0xb1a4`.
- 13 direct IDA xrefs; runtime kallsyms exposes the imported kernel helper and
  this module-local wrapper.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Semantic name of the imported helper's fixed reason value `1` in this vendor
  kernel ABI.
