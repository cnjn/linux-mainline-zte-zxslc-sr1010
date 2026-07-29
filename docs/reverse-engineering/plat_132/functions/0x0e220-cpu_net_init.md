# 0x0e220 cpu_net_init

## Status

- Status: complete
- Confidence: verified for control flow, names, object order, and failure
  behavior; strong inference for opaque NAPI/timer types.
- Size: `0x304` bytes, 186 ARM64 instructions.
- Recovered signature: `int cpu_net_init(void)`.

## Role

This function builds the Linux-facing CPU network layer after IDM IRQ setup. It
obtains four IDM TX queue records from a callback in `cpu_net_ops`, creates four netdevs,
binds four NAPI contexts, starts timers, initializes recycle state, and returns
zero on its full success path.

## Initial State and TX Queues

It copies the normal/jumbo/extra TX retrieval counts into release-ring state,
zeros four CPU-unlock state words, and sets `rls_ring_num_max = 3` only for CPU
types 133 or 129.

It then calls `idm_get_cpu_tx_q` through `cpu_net_ops + 0x20` with indices 0 to
3, storing queue-record pointers as:

| Index | Destination | Checked for null |
| --- | --- | --- |
| 0 | `omcioam_tq` | no |
| 1 | `cpu_tq` | yes |
| 2 | `idm_tq` | yes |
| 3 | `unlock_tq` and its adjacent alias | yes |

If any checked queue is null, it returns `-1`. It does not free queue records
or netdevs obtained by earlier calls.

## Netdev Registration

`cpu_net_register` is called in this fixed order:

| Type | Name | Destination |
| --- | --- | --- |
| 1 | `"sw"` | switch-facing netdev slot |
| 0 | `"pon"` | `cpu_netdev` |
| 2 | `"omci"` when `g_pon_work_mode & 0xe40`, otherwise `"oam"` | management netdev slot |
| 3 | `"idm"` | IDM netdev slot |

Any null result logs a specific failure message and returns `-1`. There is no
unregister/free path for earlier successful devices or retained TX queue records.

`cpu_net_register @ 0xb1d0` independently confirms it allocates an Ethernet
netdev, selects ordinary versus IDM netdev ops by type 3, assigns a default MAC,
registers it, and frees that individual object only if `register_netdev` fails.

## NAPI and Timers

All NAPI registrations use weight 512:

| Device | Raw slot relative to `int_info` | Poll function |
| --- | --- | --- |
| `cpu_netdev` | 0 | `cpu_net_poll` |
| `cpu_netdev` | 2 | `cpu_idm_poll` |
| `cpu_netdev` | 3 | `cpu_rls_poll` |
| IDM netdev | 1 | `idm_net_poll` |

The four contexts are contiguous 0x1a0-byte records. This slot order, rather
than registration order, is what `cpu_net_int(source)` uses.

The function then calls `testftp_init`, `net_gro_init`, and `net_gso_init`, with
no status checks. It initializes one `cpu_timer_func` timer on CPU 0 and exactly
of `jiffies + 1`.

`cpu_timer_unlock @ 0xb700` proves each unlock timer later calls
`net_check_tx_done_nolock` for its corresponding `unlock_tq` entry and
reschedules itself on `ipsec_tx_cpu`.

## Final Handoff

After `idm_recycle_init`, the function stores the address of the `cpu_netdev`
global slot, not the netdev object itself, into the global labeled `idm_netdev`.
It then clears three TX locks, logs `"pp net init ok,share 320\n"`, and returns
zero.

## Runtime Corroboration

Vendor boot logs show `pon: netif_napi_add() called with weight 512` followed by
`pp net init ok,share 320`. The captured runtime exposes `sw`, `pon`, `oam`,

## Error Behavior and Ownership

- TX queue and netdev failure paths return `-1` without unwinding prior success.
- NAPI, feature-init, timer, recycle-init, and lock-init calls are not checked.
- `idm_init` ignores this function's return value, so its failures may not
  propagate to module initialization.

## Evidence

- Full 186-instruction ARM64 disassembly at `0x0e220` through `0x0e520`.
- Direct decompilation of `cpu_net_register @ 0xb1d0`,
  `idm_recycle_init @ 0x1063c`, and `cpu_timer_unlock @ 0xb700`.
- Direct caller `idm_init @ 0x14ff4`.
- Vendor runtime dmesg and captured network interface state.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- The full `cpu_net_ops` table, TX queue ownership, NAPI context layout, and
  timer object types remain unresolved.
- No teardown path for the partially initialized netdev/timer state has been
  recovered yet.
