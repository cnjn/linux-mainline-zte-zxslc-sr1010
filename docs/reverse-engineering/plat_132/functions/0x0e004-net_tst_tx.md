# 0x0e004 net_tst_tx

## Status

- Status: complete
- Confidence: verified validation, slot selection, allocation/copy flow, drop
  accounting, return values, and direct callers; allocation-flag meaning is
  unknown.
- Size: `0xbc` bytes, 47 ARM64 instructions.
- Recovered signature:
  `int net_tst_tx(const void *data, unsigned int length, unsigned int port)`.

## Role

Construct a copied skb for one of four contiguous CPU-netdev slots and submit it
through the normal CPU TX netdev operation. This is the shared implementation
behind the OAM wrapper and the management packet test helper.

## Inputs and Return Contract

The function returns `-1` without side effects when `length == 0`, `port > 3`,
or `data == NULL`. It does not validate the selected netdev pointer.

For valid inputs it indexes the four pointer slots beginning at `cpu_netdev @
0x28158`, allocates `length + 16` bytes with allocation flags `0xa20`, and
returns zero in every allocation-success or allocation-failure case. The
unsigned addition is intentionally not overflow-checked.

On allocation failure, it calls `cpu_dev_stat(device)` and increments raw stats
word `+0x38` when non-null, identified by the established TX paths as
`tx_dropped`. It still returns zero.

## TX Ownership

After allocation, the function calls `skb_put(skb, length)`, copies the caller
buffer to the skb data pointer at `skb + 0x130`, stores the selected device at
`skb + 0x10`, and calls `cpu_net_tx(skb, device)`. Its result is intentionally
discarded. `cpu_net_tx` owns consumption of the skb on every observed path, so
this helper neither frees nor retains it after the call.

## Callers and Concurrency

- `oam_tx @ 0x0e0c0` passes fixed port 2 and propagates this status.
- `net_omci_tx_test @ 0x0e0d8` passes fixed port 2 and ignores this status.

This helper has no locks or MMIO accesses. The selected `cpu_net_tx` path
performs management-TX locking and descriptor ownership handling.

## Evidence

- Complete ARM64 body at `0x0e004` through `0x0e0bc`.
- Validation sequence `CMP W1,#0`, `CCMP W2,#3`, and `CCMP X0,XZR` at
  `0x0e004`-`0x0e010`.
- Slot load from `0x28158 + 8 * port` at `0x0e024`-`0x0e038`.
- `__netdev_alloc_skb`, `skb_put`, `memcpy`, `cpu_dev_stat`, and `cpu_net_tx`
  call sequences.
- Two direct IDA callers, plus the CPU-net initialization and TX records that
  establish slot and ownership context.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- The vendor meaning of allocation flags `0xa20` is not asserted here.
- External users of ports 0, 1, and 3 are not present in this module's direct
  call graph.
