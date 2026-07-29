# 0x0ffd8 idm_skb_stack_push

## Status

- Status: complete
- Confidence: verified flag predicate, both ownership paths, backend/FIFO
  arguments, per-CPU staging calculation, raw return-register behavior, and
  export-table entries; the void source signature is a strong inference.
- Size: `0x64` bytes, 24 ARM64 instructions.
- Recovered signature: `void idm_skb_stack_push(struct sk_buff *skb)`.

## Semantics

The function reads skb word `+0x114`. Both bit 16 and bit 0 must be set. If
either is clear, it transfers the skb directly to
`__dev_kfree_skb_any(skb, 1)`.

When both bits are set, it first calls the CPU-net backend buffer-free wrapper
with the raw `skb + 0x128` head pointer and pool zero. It then returns the skb
object itself to FIFO selection zero through:

```c
buf_fifo_free_data(skb_free_data + TPIDR_EL1, 0, skb);
```

The function has no local lock, validation, counter, or skb-field writes. The
success call leaves the FIFO helper's raw counter in the return register, and
the failure call likewise has no post-call X0 normalization. The semantic void
signature is therefore an inference from this void-style control flow, not a
verified ABI declaration.

## Caller Context

There are no direct code xrefs within `plat_132`. The module has both
`__ksymtab_idm_skb_stack_push` and `__kstrtab_idm_skb_stack_push`, so this is an
exported module interface. A scan of unresolved symbols in the supplied sibling
`kmodule/*.ko` files found no matching import; external consumers remain
possible.

## Concurrency and Ownership

No local synchronization occurs. The successful path transfers the raw head
buffer to CPU-net operation slot `+0x30` via `cpu_net_free_buf`, then transfers
the skb object to FIFO-0 staging. The reject path instead transfers the entire
skb to the fixed-reason kernel free helper. Backend and FIFO synchronization are
delegated to their callees.

## Evidence

- Complete 24-instruction ARM64 body at `0xffd8` through `0x10038`.
- `TBZ` tests of skb word `+0x114` bits 16 then 0, with the shared free path at
  `0x10024`.
- Exact success arguments: `skb[+0x128]`, pool `0`,
  `skb_free_data + TPIDR_EL1`, FIFO selection `0`, and the original skb.
- Direct reconstruction of `cpu_net_free_buf`, `__my_cpu_offset`, and
  `buf_fifo_free_data`; ELF symbol and `__ksymtab` inspection.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Original meanings of skb word `+0x114` bits 16 and 0.
- Full ownership contract of the CPU-net backend buffer-free operation.
- Source-level return type expected by external consumers.
