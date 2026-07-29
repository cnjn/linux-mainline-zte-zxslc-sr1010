# 0x0b4fc net_check_tx_done_nolock

## Status

- Status: complete
- Confidence: verified completion delta, wrap arithmetic, owner reclamation,
  queue updates, globals, and callers; raw returned register value is noted.
- Size: `0x204` bytes, 128 ARM64 instructions.
- Recovered signature:
  `u32 net_check_tx_done_nolock(struct idm_tx_queue *queue)`.

## Semantics

The function reads hardware completion through `cpu_net_ops + 0x50`
(`idm_get_tx_done(queue->hardware_queue)`) and compares it with queue word
`+0x24`. If equal, it returns that hardware value without modifying the queue.

Otherwise it computes a 16-bit wrapped completion delta:

```c
completed = hardware_done - last_done;
if (hardware_done <= last_done)
    completed += 0x10000;
```

For every completed owner slot at queue `+0x8[queue +0x18]`:

- Null owner increments global `net_txtq_err`.
- Any owner with low two bits nonzero is treated as a tagged nbuf pointer:
  `cpu_net_free_nbuf(owner & ~3)` runs and `nb_txdone_cnt` increments.
- An untagged owner on `idm_tq` is sent to `idm_skb_stack_wifi_push`.
- Other untagged owners use `dev_kfree_skb_any`.

The slot is zeroed and consumer `+0x18` advances modulo queue depth `+0x20`.
Afterward `net_tx_done_total += completed`, queue last-completion `+0x24` is
updated, and pending word `+0x14` is decremented by `completed`.

If `completed > pending`, the binary only rate-limits a diagnostic and still
performs the unsigned subtraction, so pending can underflow. The function has
no lock; callers must provide serialization.

The changed-completion path returns the old pending count left in `w0`; the
unchanged path returns hardware completion. In-module callers ignore it, so this
is residual ABI behavior rather than an observed semantic result contract.

## Callers

- `net_get_next_txdesc @ 0xce8c` opportunistically reclaims before allocation.
- `cpu_timer_func @ 0xb7a4` and `cpu_timer_unlock @ 0xb700` perform periodic
  reclamation under their respective TX locks.

## Evidence

- Complete 128-instruction ARM64 disassembly at `0xb4fc` through `0xb6fc`.
- Direct xrefs from both TX timer paths and `net_get_next_txdesc`.
- IDM ops-table mapping: `+0x50 = idm_get_tx_done`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- The exact tagged-nbuf allocation convention and Wi-Fi stack-push ownership
  contract require GSO/companion-module reconstruction.
- Queue completion indices may have wider hardware meaning despite the observed
  16-bit wrap arithmetic.
