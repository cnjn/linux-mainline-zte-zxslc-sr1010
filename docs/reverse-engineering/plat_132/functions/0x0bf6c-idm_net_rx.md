# 0x0bf6c idm_net_rx

## Status

- Status: complete
- Confidence: verified fixed queue calculation, descriptor-loop flow, jumbo
  failure path, refill/attach/free calls, delivery branches, descriptor clears,
  final update, and return behavior.
- Size: `0x328` bytes, 200 ARM64 instructions.
- Recovered signature: `int idm_net_rx(u32 count, u32 jumbo_selector)`.

## Semantics

The function computes `queue_index = 16 + jumbo_selector`, obtains that RX
queue through `cpu_net_ops + 0x18`, and consumes exactly `count` descriptors.
It always operates against the IDM netdev and always returns the original
`count` after:

```c
cpu_net_ops->flush_rx_refill();
cpu_net_ops->update_rx_queue(queue_index, count, 0);
```

Unlike `cpu_net_rx`, it does not check descriptor word 0 before translating it
to a buffer address, does not use GRO, and does not clear successful-delivery
descriptor word 0 in this caller.

## Descriptor and Buffer Paths

- Descriptor `+0x4` low 14 bits provide length.
- If descriptor `+0x5.bit6` is set, it increments the global jumbo-error
  counter, logs/dumps only while the new counter is at most five, calls
  `idm_rx_refill0(raw_buffer, 1, 1)`, clears descriptor word 0, and increments
  IDM-net drop statistics.
- For a non-jumbo descriptor, it calls `idm_rx_refill0(raw_buffer, 0, 0)`.
  A negative result clears descriptor word 0 and increments drops.
- After successful normal refill, it obtains an skb head through `net_alloc_skb`
  and calls `alloc_skb_attach_buffer(head, data - offset, normal_capacity,
  offset, 0)`. Attachment failure frees the buffer through `idm_free_buf`,
  clears descriptor word 0, and increments drops.
- On successful attachment it sets skb word `+0x114` bit 16, updates observed
  IDM-net packet/byte counters, calls `skb_put`, and assigns the IDM netdev at
  skb `+0x10`.
- If `idm_skb_recv` is non-null, it reserves 40 bytes of scratch and the helper
  writes its leading 36-byte Wi-Fi trap record with
  `idm_set_wifi_trap_info(descriptor, scratch, 16)` before delivery. Otherwise
  it uses `eth_type_trans` and `netif_receive_skb`.

The raw DMA-like address translation is the same observed formula as CPU RX:
`((u32)raw_buffer - memstart_addr) | 0xffffff8000000000ULL`.

## Relationship to Polling

`idm_net_poll` obtains queue-8 counts and calls this function twice: selector
1 for the high 16-bit count and selector 0 for the low 16-bit count. Those map
to queue indices 17 and 16. The selector is not itself trusted as a pool class;
the descriptor's bit 6 determines the explicit jumbo-error path.

## Evidence

- Complete 200-instruction ARM64 disassembly at `0xbf6c` through `0xc290`.
- Two direct callers in `idm_net_poll @ 0xc294`.
- Direct decompilation of `cpu_sw_rx @ 0xc3ec` and relevant buffer/refill
  callbacks.
- Raw IDM operations-table mapping at offsets `+0x18`, `+0x30`, `+0x38`,
  `+0x40`, and `+0x48`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Why descriptor-jumbo traffic on this path is categorically dropped needs
  hardware queue assignment or vendor source evidence.
- Exact `net_alloc_skb`/`alloc_skb_attach_buffer` ownership contract and skb
  flag-bit semantics remain unresolved.
- Successful-path descriptor clearing is likely delegated to ring update or a
  downstream owner, but this caller does not establish which.
