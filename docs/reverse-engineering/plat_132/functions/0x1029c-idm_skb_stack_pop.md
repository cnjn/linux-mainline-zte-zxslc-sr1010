# 0x1029c idm_skb_stack_pop

## Status

- Status: complete
- Confidence: verified selector-to-staging/FIFO mapping, recycle sequence,
  status-bit preservation, signed size test, release/counter path, and absence
  of direct in-module callers.
- Size: `0xb8` bytes, 44 ARM64 instructions.
- Recovered signature:
  `struct sk_buff *idm_skb_stack_pop(u8 selector, s32 minimum_size)`.

## Semantics

Uses only `selector & 1`:

- Set bit: adds `TPIDR_EL1` to `wifi1_free_data` and calls
  `buf_fifo_alloc_data(staging, 3)`.
- Clear bit: adds `TPIDR_EL1` to `wifi0_free_data` and calls
  `buf_fifo_alloc_data(staging, 2)`.

Null FIFO output returns null immediately. For a returned skb, the function
saves skb word `+0x114`, calls `skb_recycle`, then restores only bit 0 if it was
set before recycle. It tests the raw signed expression:

```c
(s64)((u64)*(u32 *)(skb + 0x120) - 64) < (s32)minimum_size
```

On a true result it frees the skb with fixed reason 1, increments the raw
short-buffer counter selected by the low selector bit, and returns null.
Otherwise it returns the recycled skb.

## Caller Context

IDA has no direct code xrefs to this entry. Its interface is likely reached
through an indirect or external path. Its two staging/FIFO selection paths match
the paired push operation, which routes objects through FIFO 2 or 3 based on the
corresponding selector bit.

## Globals and Concurrency

Reads per-CPU staging bases and updates only the short-buffer counter on the
failure path. FIFO locking is delegated to `buf_fifo_alloc_data`; no local lock
or validation protects skb layout, selector range beyond bit masking, or recycle
ownership.

## Evidence

- Complete 44-instruction ARM64 body at `0x1029c` through `0x10350`.
- Exact TPIDR_EL1/staging/FIFO selection, skb offsets, recycle call, signed
  threshold arithmetic, fixed free reason, and counter addressing.
- No direct IDA code xrefs; paired push and FIFO allocator analysis establish
  the selection context.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- External/indirect caller and minimum-size ABI contract.
- Exact semantics of skb word `+0x114` bit 0 and `skb_recycle`.
- Original names/ownership rules for the two staging regions and short counters.
