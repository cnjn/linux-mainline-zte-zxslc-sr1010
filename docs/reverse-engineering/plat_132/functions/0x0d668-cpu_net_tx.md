# 0x0d668 cpu_net_tx

## Status

- Status: complete
- Confidence: verified dispatch, policy branches, lock selection, queue/backend
  ownership, drops, return value, raw offsets, and callback slots. Descriptor
  field labels use ABI-context inference.
- Size: `0x710` bytes.
- Recovered signature:
  `netdev_tx_t cpu_net_tx(struct sk_buff *skb, struct net_device *device)`.

## Return Contract

Every exit returns zero (`NETDEV_TX_OK`). The function consumes the input skb on
all software failure paths. Hardware-ring pressure, module-readiness failure,
invalid type, PON policy failure, MIC failure, GSO failure, descriptor
exhaustion, and backend failure become local drops rather than
`NETDEV_TX_BUSY`.

Before type dispatch it tests `all_kmodules_are_already`. A false value frees
the skb, increments global `net_tx_drop`, and returns success. The switch uses
the raw netdev private type at `device + 0x888`:

| Type | Interface | Queue | Backend |
| --- | --- | --- | --- |
| 0 | `pon` | `cpu_tq` (hardware queue 1) | `cpu_net_ops + 0x68` / `idm_cpu_tx` |
| 1 | `sw` | `cpu_tq` (hardware queue 1) | `cpu_net_ops + 0x68` / `idm_cpu_tx` |
| 2 | `omci`/`oam` | `omcioam_tq` (hardware queue 0) | `cpu_net_ops + 0x78` / `idm_omci_tx` |
| 3 or other | not handled here | none | global skb drop |

Type 3 uses the separate `idm_net_tx` netdev operation.

## Locking and Queue Ownership

Types 0 and 1 serialize through `net_lock_tx`; type 2 uses `omcioam_lock_tx`.
Each lock acquisition tests `*(u32 *)(SP_EL0 + 0x10) & 0x1fff00`: nonzero uses
the raw lock path, otherwise it uses irqsave lock plus restore. Unlock is a
low-byte store-release, followed by local IRQ restoration when applicable.
Every path after acquisition unlocks.

`net_get_next_txdesc` only yields a descriptor when reclaim/backpressure state
permits it. On exhaustion the skb is freed, per-device TX drops increment, and
the function still returns `NETDEV_TX_OK`. On successful backend submission,
the skb is recorded in the TX queue owner array for completion-time free. A
negative backend result rolls back the producer index, increments drops, and
frees the skb.

Observed queue layout is descriptor base `+0`, owner array `+8`, producer `+16`,
pending `+20`, completion consumer `+24`, hardware queue ID `+28`, depth `+32`,
and last hardware completion `+36`; descriptor stride is 32 bytes.

## Per-Interface Paths

- `sw`: GSO/nonlinear skbs use `net_gso_tx(skb, device, 1)` then free the
  original skb. Direct path programs switch QoS/port descriptor state, calls
  `cpu_lowpower_tx`, then submits through `idm_cpu_tx`.
- `pon`: drops only when `!lan_up && ((g_pon_work_mode & 0xe40) ||
  !(g_pon_work_mode & 0x1a0) || !pon_is_registered())`. Otherwise it calls
  `ffe_learn_skb(skb, 3)`, uses GSO path selector zero when needed, or programs
  PON descriptor state, pads to a nominal minimum length of 60, calls
  `cpu_lowpower_tx`, and submits through `idm_cpu_tx`.
- `omci`/`oam`: when `g_pon_work_mode & 0x600`, an installed `omci_mic_add`
  callback can drop the skb. Otherwise the path programs management descriptor
  state and submits through `idm_omci_tx`.

For direct successful TX, raw device statistics at `+8`, `+24`, and `+56` are
updated as inferred `tx_packets`, `tx_bytes`, and `tx_dropped` fields. GSO
ownership uses tagged nbuf owners and frees the original skb synchronously.

## Descriptor and skb Facts

- GSO condition: `*(u16 *)(skb->head + skb->end + 4) != 0` or
  `skb->data_len != 0`, using observed skb offsets head `+0x128`, end `+0x120`,
  and data length `+0xac`.
- Direct SW/PON descriptor paths write `+0x18 = 0x08000000`, clear
  `+0x10/+0x14`, and use low 9 bits of descriptor `+0x1a` for QoS.
- SW uses optional `dev_qos_select_queue_for_lan`; PON uses optional
  `dev_qos_select_queue` only with work-mode bit `0x10`.
- OAM/OMCI programming zeroes descriptor bytes `+0x18..+0x1b`; its additional
  port/length treatment depends on work mode and is retained as a semantic
  helper pending its dedicated descriptor reconstruction.
- PON's minimum-length branch writes `skb->len = 60` after zeroing available
  linear tailroom. For nonlinear data this can change length without adding
  payload bytes.

## Evidence

- ARM64 control flow at `0xd668` through `0xdd74`; function size `0x710`.
- Netdev operations-table entries at `0x1dc80` and `cpu_net_register` type
  initialization.
- Queue accessor, completion, and concrete backend reconstructions:
  `idm_get_cpu_tx_q`, `net_check_tx_done_nolock`, `idm_cpu_tx`, and
  `idm_omci_tx`.
- TX timer and GSO xrefs for ownership/reclaim behavior.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Exact descriptor field names and FFE/QoS contracts require the dedicated
  descriptor-writer functions and companion switch-module analysis.
- Why PON padding can extend nonlinear skb length without materializing bytes is
  a verified behavior but its hardware contract is unknown.
- The original source names for the per-CPU lock-mode condition and tagged GSO
  owner convention remain unverified.
