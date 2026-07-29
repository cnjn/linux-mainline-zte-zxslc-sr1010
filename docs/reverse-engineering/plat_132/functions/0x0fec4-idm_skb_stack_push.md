# 0x0fec4 _idm_skb_stack_push

## Status

- Status: complete
- Confidence: verified all four reject predicates/counters, FIFO selection,
  staging calculation, release behavior, fixed return, and sole direct caller.
- Size: `0x114` bytes, 65 ARM64 instructions.
- Recovered signature:
  `int _idm_skb_stack_push(struct sk_buff *skb, u32 selector)`.

## Semantics

The function either queues an skb into FIFO 2 or 3 for reuse, or releases it
with `__dev_kfree_skb_any(skb, 1)`. It always returns zero.

It rejects and frees the skb in this order, incrementing one adjacent raw reject
counter per failure:

1. skb word `+0xac` is nonzero, or skb byte `+0xb6` has either bit 2 or bit 3.
2. `skb + 0x120` is below the signed 64-bit comparison threshold
   `(idmSkbRecycleLen + 127) & ~63`.
3. skb word `+0x13c` is not one.
4. If skb byte `+0xb6.bit0` is set, the low 16 bits of the raw word at
   `skb->head + skb[+0x120] + 32` are not one.

On success, it truncates the selector to a byte. Zero uses
`wifi0_free_data + TPIDR_EL1` and FIFO selection 2; any nonzero byte uses
`wifi1_free_data + TPIDR_EL1` and FIFO selection 3. It forwards the skb through
`buf_fifo_free_data` and discards that helper's raw counter return.

## Caller Context

`idm_skb_stack_wifi_push @ 0x103dc` is the sole direct caller. That wrapper
selects input selector zero for skb word `+0x114.bit17` or one for bit 18, so the
FIFO 2/3 split is driven by completion-reclaim metadata.

## Concurrency and Ownership

No local lock protects the raw skb reads or reject counters. FIFO synchronization
is delegated to `buf_fifo_free_data`. Every reject transfers skb ownership to
the fixed-reason free helper; success transfers it to FIFO staging.

## Evidence

- Complete 65-instruction ARM64 body at `0xfec4` through `0xffd4`.
- Exact skb offsets, counter increments, signed threshold arithmetic, selector
  byte truncation, per-CPU staging bases, and FIFO selections.
- Sole direct caller xref and paired FIFO push analysis.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Original meanings of raw skb fields/metadata checks and the four counters.
- Ownership/lifetime rules for FIFO 2/3 recycled skb entries.
