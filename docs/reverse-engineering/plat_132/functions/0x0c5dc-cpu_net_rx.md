# 0x0c5dc cpu_net_rx

## Status

- Status: complete
- Confidence: verified descriptor-loop control flow, raw field offsets,
  operations-table calls, buffer ownership transitions, delivery branches, and
  final ring update. Descriptor-field names are explicitly marked as inferred.
- Size: `0x544` bytes, 335 ARM64 instructions.
- Recovered signature:
  `int cpu_net_rx(u32 count, u32 queue, u32 jumbo_selector)`.

## Queue and Loop Contract

The function computes `queue_index = queue + 8 * jumbo_selector`, obtains its
RX queue through `cpu_net_ops + 0x18` (`idm_get_cpu_rx_qc`), and calls
`get_next_rxdesc` once for every requested descriptor. It selects `cpu_netdev`
only when `jumbo_selector == 1`; every other selector uses `sw_netdev`.

After all iterations, regardless of delivery/drop outcome, it calls:

```c
cpu_net_ops->flush_rx_refill();
cpu_net_ops->update_rx_queue(queue_index, count, jumbo_descriptor_count);
return count;
```

Thus the return value is the requested descriptor count, not the number of
successfully delivered packets. `idm_rx_update` is the verified `+0x48` table
slot; it publishes queue consumption after the refill flush.

## Raw Descriptor Fields

| Offset | Observed use | Confidence |
| --- | --- | --- |
| `+0x0` | 32-bit raw buffer word; cleared on most terminal paths | verified |
| `+0x4` | low 14 bits are received length | verified |
| `+0x5.bit6` | refill/free pool selector and jumbo descriptor counter | verified use; pool meaning inferred |
| `+0x5.bit7` | management/OMCI-OAM branch | verified branch; protocol meaning inferred |
| `+0x6 & 0x3f` | 64-entry RX histogram index and optional skb byte | verified |
| `+0x8` | testftp/GRO discriminator | verified use |
| `+0xd` | payload-relative offset for testftp validation | verified |
| `+0xe & 0x15` | GRO eligibility bits | verified use |

The raw buffer translates to a virtual address using
`((u32)raw_buffer - memstart_addr) | 0xffffff8000000000ULL`.

## Buffer Ownership and Delivery

- A zero raw buffer increments the selected device's observed drop counter and
  calls `idm_rx_refill0(0, pool, 0)`.
- A management descriptor calls `cpu_omci_rx`; it updates management-device
  packet/byte or drop statistics, calls `idm_rx_refill0(raw_buffer, pool, 1)`,
  then clears the raw word.
- Ordinary RX first calls `idm_rx_refill0(raw_buffer, pool, 0)`. A negative
  result increments the selected device drop counter and clears the raw word.
  The refill helper itself requeues the old buffer on allocation failure.
- Successful ordinary RX updates selected-device packet/byte counters. A
  matching testftp frame reports through `testftp_net_report`, frees its buffer
  through `cpu_net_ops + 0x30` (`idm_free_buf`), and clears the descriptor.
- GRO is eligible only with descriptor `+0xe & 0x15 == 0x15`, `+0x8 != 0xfd`,
  non-jumbo pool, non-null `switch_skb_recv`, and enabled `net_gro_en`. A
  nonzero `pp_net_tcp_gro` result skips the caller's descriptor clear.
- The ordinary skb path flushes GRO, attaches the existing buffer to an skb,
  sets device and observed skb metadata fields, then either calls `cpu_sw_rx`
  or executes `eth_type_trans` plus `netif_receive_skb`. It clears descriptor
  word 0 afterward. Attachment failure frees the buffer, clears the descriptor,
  and increments drops.

Observed device statistics update at raw offsets `+0x0`, `+0x10`, and `+0x30`;
their packet/byte/drop labels are strong ABI-context inference.

## Operations Table Use

| Offset | Function | Use here |
| --- | --- | --- |
| `+0x18` | `idm_get_cpu_rx_qc` | get RX queue by computed index |
| `+0x30` | `idm_free_buf` | return buffer after testftp/attach failure |
| `+0x38` | `idm_rx_refill_flush` | final flush |
| `+0x40` | `idm_rx_refill0` | empty/management/ordinary refill |
| `+0x48` | `idm_rx_update` | publish final queue consumption |

## Evidence

- Complete 335-instruction ARM64 disassembly at `0xc5dc` through `0xcb1c`.
- Four direct callers: normal and jumbo branches in `cpu_net_poll` and
  `cpu_idm_poll`.
- Direct decompilation of `idm_rx_refill0 @ 0x14144`, `idm_net_rx @ 0xbf6c`,
  `cpu_sw_rx @ 0xc3ec`, and `cpu_omci_rx @ 0xc4b0`.
- Raw IDM ops table mapping recorded in `MEMORY.md`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Exact descriptor type, all flag meanings, and the reason successful GRO leaves
  descriptor word 0 untouched require `pp_net_tcp_gro` reconstruction.
- Exact skb field names at `+0x108`, `+0x114`, and `+0xf8` remain ABI-context
  inference.
- Debug-only dump/rate-limit branches are not reproduced in the semantic C, but
  their functional paths were included in control-flow analysis.
