# 0x1063c idm_recycle_init

## Status

- Status: complete
- Confidence: verified possible-CPU iteration, per-CPU raw stores, FIFO reset,
  masks, allocation arithmetic, failure behavior, caller, and semantic void ABI.
- Size: `0xfc` bytes, 60 ARM64 instructions.
- Recovered signature: `void idm_recycle_init(void)`.

## Role

Initialize four software recycle FIFOs and selected per-CPU staging state after
CPU netdev/NAPI setup.

## Semantics

The function iterates possible CPU IDs via `cpumask_next(-1,
__cpu_possible_mask)` until the unsigned result reaches `nr_cpu_ids`. For each
CPU's `__per_cpu_offset`, it zeros two raw 32-bit words at offsets `+0x200` and
`+0x204` in each of `skb_free_data`, `kmem_free_data`, `wifi0_free_data`, and
`wifi1_free_data`.

It then zeros exactly 128 bytes covering four 32-byte `buf_fifo` records. For
each FIFO it sets mask `+0x8` to `0xfff`, clears lock word `+0x10`, and stores an
unchecked `__kmalloc(8 * (mask + 1), 0xa20)` result at entry-array pointer
`+0x18`. Thus each normal initialized ring requests 4096 pointer slots.

Allocation failures are stored as null entries with no error propagation,
cleanup, retry, or rollback. The raw machine return is the last allocation
result, ignored by its caller; semantic ABI is void.

## Caller Context

The sole direct caller is `cpu_net_init @ 0x0e220`, after timer setup and before
publishing the CPU-netdev slot and clearing TX locks.

## Evidence

- Complete ARM64 body at `0x1063c` through `0x10734`.
- `cpumask_next`, `nr_cpu_ids`, and `__per_cpu_offset` iteration sequence.
- Eight raw per-CPU zero stores per possible CPU.
- Exact `memset(..., 0, 0x80)`, four `0xfff` masks, lock clears, allocation
  flags `0xa20`, `8 * (mask + 1)` arithmetic, and entry-pointer stores.
- Sole direct CPU-net initialization caller.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Exact roles of each per-CPU raw word at `+0x200/+0x204` and why only those
  words are reset here.
- Teardown behavior for the four entry arrays is not recovered.
