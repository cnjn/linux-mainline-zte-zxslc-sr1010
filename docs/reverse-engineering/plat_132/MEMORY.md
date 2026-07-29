# plat_132.ko Analysis Memory

This file is the durable handoff record for context compression and future
analysis sessions. Read it before resuming work and update it after each
completed function or durable architectural discovery.

## Artifact and Session

- Target path:
  `vendor-reference/sr1010-vendor-runtime/modules/files/kmodule/plat_132.ko`
- SHA-256:
  `6cb3e7c9567b4549b5d1b1c6d87f502d3b07fcde0461c2d28a5ad115661d4d51`
- ELF: ARM64, relocatable, not stripped, image base `0x0`.
- Module metadata: `plat_132`, `Dual BSD/GPL`, Linux 5.4.196 ARM64.
- IDA database session used for reconstruction: `plat132-analysis-reopen`.
- IDA survey: 656 functions, 649 named, 1,094 strings, 20 segments.
- Do not overwrite or reset existing IDA working files. The shared worktree
  already contains modified and untracked IDA artifacts.
- Hard boundary: `linux-6.18.38/` is a mainline source tree, not vendor source.
  Exclude it from all vendor-behavior, type, register, and control-flow
  inference. Use only the vendor binary, vendor runtime collection, and needed
  companion vendor modules as implementation evidence.
- `/Volumes/code/linux-5.4.196` is available as an upstream ABI reference only.
  It can help identify standard Linux 5.4.196 callbacks or candidate structure
  layouts, but the vendor kernel is expected to be heavily modified. Never use
  it as proof of vendor behavior, offsets, register definitions, ownership, or
  omitted code paths; label every such use as `upstream ABI reference`.

## Runtime Identity

- The live target identifies itself as ZTE 133 and uses the
  `zte,zx279133-pon` platform binding.
- `plat_132` supports multiple vendor SoC variants, including code paths for
  129, 132, and 133; its file name is not proof that the runtime SoC is 132.
- Runtime module stack includes `plat_132`, `np`, `switch`, `zx_ponreg`,
  `peripheral`, and PHY-related modules.
- Runtime PON work mode is `0x10`, meaning P2P mode. In this mode `cpu_net_init`
  registers `oam`, not `omci`.

## Architectural Model

- This is an integrated network-platform driver, not a conventional standalone
  PCIe or single-MAC Ethernet driver.
- Layers: PHY/SerDes -> SMAC/XMAC -> NPPT hardware forwarding -> IDM CPU DMA
  queues -> Linux logical netdev and NAPI interfaces.
- The Linux-facing logical devices are `pon` (ID 0), `sw` (ID 1), `oam` or
  `omci` (ID 2), and `idm` (ID 3).
- `sw` and `pon` are CPU-side injection/trap interfaces. Physical port and
  bridge policy is coordinated with companion switch modules.
- `cpu_net_netdev_ops` has `ndo_open=cpu_net_open`,
  `ndo_stop=cpu_net_stop`, and `ndo_start_xmit=cpu_net_tx`; `idm` swaps only
  the transmit callback to `idm_net_tx`.

## Lifecycle Facts

- `init_module @ 0x1c3b4` sets the 133 CPU type, registers the PON platform
  driver, then calls `nppt_init @ 0x11a50`.
- `zx_pon_probe @ 0x0580` maps PON, system control, top CRM, pinmux, efuse,
  NPPT, RGMII, SerDes, XMAC PCS, and GEPHY regions; it acquires PON, NPPT, and
  four IDM IRQs.
- `nppt_init` calls `sipc_init`, `greg_init`, `nppt_smac_init`, and `idm_init`.
- `idm_init @ 0x14ff4` configures the IDM hardware and calls
  `cpu_register_netinfo` followed by `cpu_net_init`.
- Runtime IDM reserved memory begins at `0x9d700000`, is `0xa00000` bytes, and
  the driver logged a calculated requirement of `0x813800` bytes.

## Completed Functions

- `init_module @ 0x1c3b4`: complete and recorded in
  `functions/0x1c3b4-init_module.md`; source-like C is in
  `recovered/plat_module.c`.
- Verified machine-code flow: set `g_pon_cputype=2`, print the CPU-133 message,
  call `pon_driver_register`, return immediately on nonzero status, otherwise
  return `nppt_init()`.
- `isCpuType_133 @ 0x1dc` verifies `2` means 133. Related verified values are
  132=`1` and 129=`4`.
- Preserve the observed lack of platform-driver unregister if `nppt_init`
  fails. Do not invent cleanup until the exit path is reconstructed.
- `pon_driver_register @ 0x0e8c`: complete and recorded in
  `functions/0x0e8c-pon_driver_register.md`; source-like C is in
  `recovered/plat_module.c`.
- Verified call: `__platform_driver_register(&zx_pon_driver, &__this_module)`
  with unchanged status return. `zx_pon_driver` is at `0x26438`; observed
  fields point to `zx_pon_probe @ 0x0580`, `zx_pon_remove @ 0x0308`, name
  `zte,zx279133-pon`, module owner, and `zx_pon_match @ 0x1d218`.
- The 5.4.196 upstream `struct platform_driver` layout was used only to label
  candidate field names. Binary values, xrefs, and vendor strings are the
  implementation evidence.
- `zx_pon_probe @ 0x0580`: complete and recorded in
  `functions/0x0580-zx_pon_probe.md`; source-like C is in
  `recovered/plat_probe.c`.
- It enumerates all matching vendor OF nodes only on CPU 133 or 129, maps the
  PON, PPS, NPPT, RGMII, IDM, XMAC PCS, SerDes, PCU, and GEPHY resources, then
  initializes clocks/reset/SerDes and registers PON plus NPPT IRQ handlers.
- Work-mode bit mapping is verified: GPON=0x40->5, XGPON=0x200->6,
  XGPONS=0x400->7, EPON=0x20->0, XEPON=0x80->1, XEPONS=0x100->4,
  P2P=0x10->15; fallback->7. Runtime was P2P and corrected `lan_up_port` to 6.
- Required resource failures return literal `-19`; no observed mapping or IRQ
  unwind occurs. `register_nppt_int` failure does not free a successful PON IRQ.
- Preserve the low-power alias ioremap calls inside the OF-node loop. They map
  0x10e10000, 0x16100000, and 0x10e20000 for every matched node and do not
  visibly check their results.

## IDM Operations Table

`idm_init` installs `idm_ops` as `cpu_net_ops`. Confirmed offsets are:

| Offset | Function |
| --- | --- |
| `+0x00` | `idm_int_disable` |
| `+0x08` | `idm_int_enable` |
| `+0x10` | `idm_get_cpu_rx_cnt` |
| `+0x18` | `idm_get_cpu_rx_qc` |
| `+0x20` | `idm_get_cpu_tx_q` |
| `+0x28` | `idm_alloc_nbuf` |
| `+0x30` | `idm_free_buf` |
| `+0x38` | `idm_rx_refill_flush` |
| `+0x40` | `idm_rx_refill0` |
| `+0x48` | `idm_rx_update` |
| `+0x50` | `idm_get_tx_done` |
| `+0x58` | `idm_get_reorder_rls` |
| `+0x60` | `idm_rls_update` |
| `+0x68` | `idm_cpu_tx` |
| `+0x70` | `idm_cpu_nb_tx_update` |
| `+0x78` | `idm_omci_tx` |
| `+0x80` | `idm_wifi_tx` |

## RX, TX, and Reclamation Facts

- `cpu_net_rx @ 0xc5dc` reads IDM descriptors, converts buffer addresses from
  the reserved pool, refills before handing ownership upward, and attaches the
  external data buffer with `alloc_skb_attach_buffer`.
- Normal packets go through `switch_skb_recv` when installed, otherwise
  `eth_type_trans` plus `netif_receive_skb`.
- Management descriptors go through `cpu_omci_rx`; IDM/Wi-Fi traps can go
  through `idm_skb_recv` with `idm_set_wifi_trap_info` metadata.
- `cpu_net_tx @ 0xd668` branches by logical port ID. It handles `sw`, `pon`,
  and OAM/OMCI traffic; `idm_net_tx @ 0xd234` handles IDM traffic.
- `idm_cpu_tx @ 0x14a30`, `idm_omci_tx @ 0x1493c`, and
  `idm_wifi_tx @ 0x14be4` write physical data addresses into descriptors,
  issue a data barrier, and ring different hardware doorbells.
- `net_check_tx_done_nolock @ 0xb4fc` uses `idm_get_tx_done` to reclaim skb or
  vendor buffers. `cpu_timer_func @ 0xb7a4` performs periodic completion checks.
- Code uses `virt_to_phys` and barriers rather than visible generic
  `dma_map_*` calls. Do not infer cache/IOMMU behavior beyond that evidence.

## Interrupt and NAPI Facts

- `idm_cfg_int @ 0x14d88` requests four IRQs: CPU, IDM, buffer-release, and
  localtest. The handlers mask the matching IDM bit before NAPI scheduling.
- IRQ 26 `cpu` -> `idm_cpu_int` -> `cpu_net_int(0)` -> `cpu_net_poll`.
- IRQ 27 `idm` -> `idm_wifi_int` -> `cpu_net_int(1)` -> `idm_net_poll`.
- IRQ 28 `buf_rls` -> `idm_rls_int` -> `cpu_net_int(3)` -> `cpu_rls_poll`.
- IRQ 29 `localtest` -> `idm_all_int` -> `cpu_net_int(2)` -> `cpu_idm_poll`.
- NAPI slots are contiguous at `int_info + 0x1a0 * source`: slots 0, 1, 2, 3
  correspond to CPU poll, IDM-net poll, CPU-IDM poll, and release poll.
- NAPI completion calls the IDM enable operation to re-enable the relevant
  interrupt source.
- Runtime interrupt snapshot recorded 268 CPU IRQs and zero for the other IDM
  IRQs at collection time.

## MAC, PHY, and SerDes Facts

- `nppt_smac_init @ 0x129c8` initializes SMACs and chooses XMAC PHY/work-mode
  configuration.
- `smac_check_phy_task_thread @ 0x12890` polls PHY indexes 0 through 6 every
  100 ms; `check_phy @ 0x126e4` reconfigures and enables/disables MACs when
  state changes.
- `xmac_init_by_work_mode @ 0x17da0` maps work modes as follows:
  0=10GBASE-R, 1=5GBASE-R, 2=1GBASE-X, 3=SGMII, 4=2.5GBASE-X,
  5=10G USXGMII auto, 6=5G USXGMII auto, 7=2.5G USXGMII auto,
  8/9=HSGMII variants.
- Runtime log shows XMAC0 configured in mode 5 and XMAC1 in mode 4, followed
  by a 1G full-duplex PHY link update on MAC 5.

## Cross-Module Boundaries

- Exported callback slots in `plat_132`: `switch_skb_recv`, `idm_skb_recv`,
  `idm_recv_cmpl`, `dev_qos_select_queue_for_lan`, and
  `all_kmodules_are_already`.
- `regisetr_omci_mic_add_handle` registers the OMCI MIC-add callback.
- `ffe_learn_skb` is an imported external function used by the PON TX path.
- Do not attribute implementation details of these hooks to `plat_132` until
  the responsible companion module is independently analyzed.

## Evidence Locations

- Module: `vendor-reference/sr1010-vendor-runtime/modules/files/kmodule/plat_132.ko`
- Runtime boot and driver log: `vendor-reference/sr1010-vendor-runtime/kernel/dmesg.txt`
- Runtime interrupts: `vendor-reference/sr1010-vendor-runtime/system/proc/interrupts`
- Runtime interfaces: `vendor-reference/sr1010-vendor-runtime/network/proc-net-dev.txt`
- Runtime modules: `vendor-reference/sr1010-vendor-runtime/system/proc/modules`
- Runtime symbols: `vendor-reference/sr1010-vendor-runtime/system/proc/kallsyms`

## Outstanding Questions

- Exact IDM descriptor bitfield names and register semantics require either a
  hardware reference manual or corroboration from companion modules.
- The full policy behind `switch_skb_recv`, `idm_skb_recv`, QoS callbacks, and
  FFE learning is outside this module.
- Reconstructed C must remain semantic pseudocode until vendor kernel headers,
  MMIO definitions, and configuration contracts are recovered.

## Next Action

- `zx_pon_remove @ 0x0308`: complete and recorded in
  `functions/0x0308-zx_pon_remove.md`; source-like C is in
  `recovered/plat_probe.c`.
- It calls only `unregister_pon_int` then `unregister_nppt_int`, which invoke
  `free_irq(g_pon_irq, &pon_int_info)` and
  `free_irq(g_nppt_irq, &pon_int_info)`, then returns zero. Do not attribute
  IDM IRQ or mapping cleanup to this callback without reconstructing `nppt_exit`.
- `nppt_init @ 0x11a50`: complete and recorded in
  `functions/0x11a50-nppt_init.md`; source-like C is in
  `recovered/plat_module.c`.
- It always calls `sipc_init`, `greg_init`, `nppt_smac_init`, and `idm_init` in
  that order, returns their bitwise OR, and does no rollback. `sipc_init`,
  `greg_init`, and `nppt_smac_init` return zero; IDM is the remaining observed
  nonzero source.
- `register_pon_int @ 0x16a0`: complete and recorded in
  `functions/0x16a0-register_pon_int.md`; source-like C is in
  `recovered/plat_irq.c`.
- It requests `g_pon_irq` with hard handler `zx_pon_int`, no threaded handler,
  zero flags, label `pon`, and `&pon_int_info`; it returns negative errors but
  normalizes nonnegative results to zero. Its paired unregister uses the same
  IRQ/dev-id pair.
- `register_nppt_int @ 0x1710`: complete and recorded in
  `functions/0x1710-register_nppt_int.md`; source-like C is in
  `recovered/plat_irq.c`.
- It is control-flow-identical to PON IRQ registration but uses `g_nppt_irq`,
  `zx_nppt_int`, label `nppt`, and the same `&pon_int_info` dev-id. Its paired
  unregister uses the same IRQ/dev-id pair.
- `pon_int_info` is a shared mutable callback-context slot. The top-level PON
  and NPPT IRQs use its address as `dev_id`; protocol callback registration
  helpers replace its stored context value before unmasking their event bit.
- `zx_pon_int @ 0x12bc`: complete and recorded in
  `functions/0x12bc-zx_pon_int.md`; source-like C is in
  `recovered/plat_irq.c`.
- It computes `*(u32 *)(pon_base + 0x40) & ~*(u32 *)(pon_base + 0x44)`,
  dispatches GPON/XGPON/EPON/XEPON/XEDPON/LP/low-power callbacks independently,
  updates PON registration state, and always returns 1. The low-power callback
  is intentionally invoked without a null test. The DGi bit runs a guarded,
  mode-specific recovery sequence; `dg_isr` is stored by its registration API
  but not called by this dispatcher.
- `zx_nppt_int @ 0x0ed4`: complete and recorded in
  `functions/0x0ed4-zx_nppt_int.md`; source-like C is in
  `recovered/plat_irq.c`.
- It computes `*(u32 *)(nppt_base + 0x0) & ~*(u32 *)(nppt_base + 0x4)`, then
  processes OAM (bit `0x100`), PTP (bit `0x400`), and PTP stamp (bit `0x200`)
  in that order and always returns 1. OAM sets `soam_alarm_flag` and performs
  11 volatile reads; OAM/PTP receive `(0, 0)`, while PTP stamp receives the
  shared callback-context value.
- `idm_init @ 0x14ff4`: complete and recorded in
  `functions/0x14ff4-idm_init.md`; source-like C is in
  `recovered/plat_idm.c`.
- It obtains vendor reserved memory, validates a calculated capacity, configures
  24 RX and four TX queues, fills normal/jumbo software FIFOs, creates two
  unchecked slab caches, programs raw IDM registers at `nppt_base + 0x280000`,
  refills RX rings in 2048-entry batches, configures IDM IRQs, and starts CPU
  networking. All local failures return `-1` without local rollback; success
  ignores `cpu_net_init`'s result.
- Runtime boot evidence: available reserved memory `0xa00000`, calculated need
  `0x813800`, base `0x9d700000`, normal/jumbo cache sizes 2432/10176, and RX
  buffer counts `0x800`/`0x20` with BP counts `0x1000`/`0x1000`.
- `idm_cfg_int @ 0x14d88`: complete and recorded in
  `functions/0x14d88-idm_cfg_int.md`; source-like C is in
  `recovered/plat_idm.c`.
- It writes four IDM words, requests `g_idm_irq[0..3]` as `cpu`, `idm`,
  `buf_rls`, and `localtest`, propagates negative request statuses without
  unwinding earlier registrations, and then records affinity targets 1/2/2/3
  on four CPUs or 0/1/0/1 on two CPUs. The affinity bitmap pointer pattern is
  strongly inferred from `cpu_bit_bitmap` xrefs; original import declarations
  remain unresolved.
- `idm_int_enable @ 0x13a78`: complete and recorded in
  `functions/0x13a78-idm_int_enable.md`; source-like C is in
  `recovered/plat_idm.c`.
- It clears requested bits in `idm_int_mask`, writes `nppt_base + 0x280040`,
  and performs the update under local IRQ save plus a raw `idm_lock_int` lock.
  Its paired disable helper ORs bits into the same mask, establishing that set
  bits mask sources.
- `idm_int_disable @ 0x13ad8`: complete and recorded in
  `functions/0x13ad8-idm_int_disable.md`; source-like C is in
  `recovered/plat_idm.c`.
- All four IDM hard IRQ handlers call it before dispatching into `cpu_net_int`;
  it ORs source bits into `idm_int_mask` and writes the same hardware mask
  register under the same local-IRQ/raw-lock protection.
- `idm_cpu_int @ 0x13bb0`: complete and recorded in
  `functions/0x13bb0-idm_cpu_int.md`; source-like C is in
  `recovered/plat_idm.c`.
- It ignores generic IRQ arguments, masks `idm_info + 0x0`, then calls
  `cpu_net_int(0)` and returns 1. `cpu_net_int` maps index 0 to `int_info` and
  schedules or accounts for its NAPI context.
- `idm_wifi_int @ 0x13b88`: complete and recorded in
  `functions/0x13b88-idm_wifi_int.md`; source-like C is in
  `recovered/plat_idm.c`.
- It is the source-1 counterpart: masks `idm_info + 0x4`, calls
  `cpu_net_int(1)`, and returns 1.
- `idm_rls_int @ 0x13b60`: complete and recorded in
  `functions/0x13b60-idm_rls_int.md`; source-like C is in
  `recovered/plat_idm.c`.
- It is the source-3 counterpart: masks `idm_info + 0xc`, calls
  `cpu_net_int(3)`, and returns 1.
- `idm_all_int @ 0x13b38`: complete and recorded in
  `functions/0x13b38-idm_all_int.md`; source-like C is in
  `recovered/plat_idm.c`.
- It is the source-2 counterpart: masks `idm_info + 0x8`, calls
  `cpu_net_int(2)`, and returns 1.
- `idm_get_cpu_rx_qc @ 0x13088`: complete and recorded in
  `functions/0x13088-idm_get_cpu_rx_qc.md`; source-like C is in
  `recovered/plat_idm.c`.
- It returns `&idm_rx_q[index]` with an unchecked 16-byte stride and is exposed
  through the IDM ops table, corroborating the RX queue layout in `idm_init`.
- `idm_get_cpu_tx_q @ 0x1309c`: complete and recorded in
  `functions/0x1309c-idm_get_cpu_tx_q.md`; source-like C is in
  `recovered/plat_idm.c`.
- It returns `&idm_tx_q[index]` at a 40-byte stride only for indices 0 through
  3; larger indices return null. It is also exposed through the IDM ops table.
- `idm_get_cpu_rx_cnt @ 0x137fc`: complete and recorded in
  `functions/0x137fc-idm_get_cpu_rx_cnt.md`; source-like C is in
  `recovered/plat_idm.c`.
- It reconstructs indices 0 through 7 from paired 16-bit register fields at
  `nppt_base + 0x280000`; larger indices read a raw word at
  `4 * ((index + 0x31) & 0x3fffffff)`. It has no range or snapshot protection.
- `idm_get_tx_done @ 0x13864`: complete and recorded in
  `functions/0x13864-idm_get_tx_done.md`; source-like C is in
  `recovered/plat_idm.c`.
- It returns a raw low 16-bit value from special offset `0x84` for index zero
  or `4 * ((index + 0x2a) & 0x3fffffff)` for nonzero indices.
- `idm_rx_refill0 @ 0x14144`: complete and recorded in
  `functions/0x14144-idm_rx_refill0.md`; source-like C is in
  `recovered/plat_idm.c`.
- It either reuses an old buffer directly or allocates a new one and stages a
  byte-swapped physical payload address in a per-CPU 2 x 32-entry refill area.
  A 32-entry batch invokes the lock-protected flush helper. Allocation failure
  returns `-1` even when a supplied old buffer is requeued.
- `cpu_register_netinfo @ 0x0e1ec`: complete and recorded in
  `functions/0x0e1ec-cpu_register_netinfo.md`; source-like C is in
  `recovered/plat_idm.c`.
- It copies the five words/pointer in the 24-byte `idm_info` record to CPU-net
  globals and returns the `+0xc` word, which `idm_init` ignores.
- `cpu_net_init @ 0x0e220`: complete and recorded in
  `functions/0x0e220-cpu_net_init.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It obtains four IDM TX queue records through `idm_get_cpu_tx_q`, then creates
  `sw`/`pon`/`omci-or-oam`/`idm` netdevs, four weight-512 NAPI contexts, one CPU
  timer, and two unlock timers. It has no local rollback and `idm_init` ignores
  its `-1` failures.
- `cpu_net_register @ 0x0b1d0`: complete and recorded in
  `functions/0x0b1d0-cpu_net_register.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It allocates a netdev, selects IDM ops only for type 3, copies a name/default
  MAC, registers it, and frees only that object on negative registration status.
- `cpu_net_open @ 0x0b8d0`: complete and recorded in
  `functions/0x0b8d0-cpu_net_open.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It atomically clears a nested netdev state bit, enables carrier, then enables
  NAPI and calls `idm_int_enable` through `cpu_net_ops + 0x8` only for `pon` and
  `idm` devices.
- `cpu_net_stop @ 0x0b9b0`: complete and recorded in
  `functions/0x0b9b0-cpu_net_stop.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It atomically sets the same nested state bit, turns carrier off, disables
  selected NAPI contexts, and calls `idm_int_disable` through `cpu_net_ops + 0`.
- `cpu_net_timeout @ 0x0b2a8`: complete and recorded in
  `functions/0x0b2a8-cpu_net_timeout.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It wakes the netdev queue at `device + 0x3c0` then conditionally refreshes
  its `+0x88` timestamp from `jiffies`; it is installed in both CPU and IDM
  netdev-ops tables.
- `cpu_net_int @ 0x0e188`: complete and recorded in
  `functions/0x0e188-cpu_net_int.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It selects `int_info + 0x1a0 * source`, increments its `+0x18c` counter, and
  conditionally schedules it through NAPI; already-scheduled work increments
  `+0x190`. It has no source range check.
- `cpu_net_poll @ 0x0cce4`: complete and recorded in
  `functions/0x0cce4-cpu_net_poll.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It makes at most four high-to-low RX queue passes, decodes normal/jumbo counts
  from `idm_get_cpu_rx_cnt`, routes enabled classes to `cpu_net_rx`, flushes
  GRO, then completes NAPI and unmasks source word 0 only with remaining budget.
- `cpu_idm_poll @ 0x0cb20`: complete and recorded in
  `functions/0x0cb20-cpu_idm_poll.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It uses NAPI slot 2 and source word 8, scans only queues enabled by that word,
  makes at most four passes, never flushes GRO, and completes/re-enables only
  with remaining budget.
- `idm_net_poll @ 0x0c294`: complete and recorded in
  `functions/0x0c294-idm_net_poll.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It uses slot 1/source word 4, polls fixed queue 8 for up to four passes,
  invokes optional `idm_recv_cmpl` on every exit, then completes/re-enables only
  with remaining budget.
- `cpu_rls_poll @ 0x0b86c`: complete and recorded in
  `functions/0x0b86c-cpu_rls_poll.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It ignores budget, locks `idm_lock_tx`, runs reorder release, uses a low-byte
  store-release unlock, completes slot 3, re-enables source word c, and returns
  1.
- `cpu_net_rx @ 0x0c5dc`: complete and recorded in
  `functions/0x0c5dc-cpu_net_rx.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It consumes requested descriptors from `queue + 8 * jumbo_selector`, handles
  empty/management/ordinary/testftp/GRO/skb paths, flushes refills, updates the
  queue, and always returns the requested count. Successful GRO leaves its raw
  descriptor word untouched in this caller.
- `idm_net_rx @ 0x0bf6c`: complete and recorded in
  `functions/0x0bf6c-idm_net_rx.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It maps selectors to fixed queues 16/17, drops descriptor-jumbo frames after
  reuse refill, attaches normal buffers to skbs, flushes/updates its queue, and
  always returns the requested count.
- `cpu_sw_rx @ 0x0c3ec`: complete and recorded in
  `functions/0x0c3ec-cpu_sw_rx.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It routes a raw descriptor-byte predicate to IDM trap delivery when available;
  otherwise it calls the already-validated switch skb callback.
- `cpu_omci_rx @ 0x0c4b0`: complete and recorded in
  `functions/0x0c4b0-cpu_omci_rx.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It treats a missing OMCI/OAM callback as success, validates MIC only in work
  modes matching `0xe40`, stores the OMCI port locally, and clamps OAM ports to
  0 through 7.
- `cpu_net_tx @ 0x0d668`: complete and recorded in
  `functions/0x0d668-cpu_net_tx.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It dispatches SW/PON/OMCI-OAM by raw netdev type, serializes queue ownership,
  and always returns success while converting every local failure to an skb
  drop. Direct TX success retains the skb for completion reclaim.
- `idm_net_tx @ 0x0d234`: complete and recorded in
  `functions/0x0d234-idm_net_tx.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It uses GSO only when configured, otherwise locks the IDM TX queue, submits
  through `idm_wifi_tx`, and returns -1 only when no descriptor is available.
- `cpu_net_pon_set_desc @ 0x0d5ac`: complete and recorded in
  `functions/0x0d5ac-cpu_net_pon_set_desc.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It writes the PON descriptor control/QoS fields and sets skb byte `+0x108`
  from the PON/LAN state machine.
- `idm_cpu_tx @ 0x14a30`: complete and recorded in
  `functions/0x14a30-idm_cpu_tx.md`; source-like C is in
  `recovered/plat_idm.c`.
- It writes physical data and encoded length/port fields, then uses DSB ST plus
  the first TX doorbell; it always returns zero.
- `idm_omci_tx @ 0x1493c`: complete and recorded in
  `functions/0x1493c-idm_omci_tx.md`; source-like C is in
  `recovered/plat_idm.c`.
- It enforces a 15-byte minimum, programs the OMCI descriptor pattern, then
  submits through the management TX doorbell and returns zero.
- `idm_wifi_tx @ 0x14be4`: complete and recorded in
  `functions/0x14be4-idm_wifi_tx.md`; source-like C is in
  `recovered/plat_idm.c`.
- It pads short frames, encodes skb port metadata into the Wi-Fi descriptor, and
  submits through the third TX doorbell with an unconditional zero return.
- `net_check_tx_done_nolock @ 0x0b4fc`: complete and recorded in
  `functions/0x0b4fc-net_check_tx_done_nolock.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It reclaims wrapped hardware completions using tagged-nbuf, IDM Wi-Fi-stack,
  or skb-free paths, then updates queue consumer/pending state without locking.
- `cpu_timer_func @ 0x0b7a4`: complete and recorded in
  `functions/0x0b7a4-cpu_timer_func.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It conditionally reclaims three TX queues, always performs IDM reorder
  release, and re-arms the global timer at `jiffies + 1` on CPU 0.
- `net_gso_tx @ 0x0f87c`: complete and recorded in
  `functions/0x0f87c-net_gso_tx.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It always returns zero after routing upload or TCP GSO work and accounting
  success/drop; TX callers free the original skb after it returns.
- `pp_net_tcp_gro @ 0x10eac`: complete and recorded in
  `functions/0x10eac-pp_net_tcp_gro.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It validates descriptor/IP length, filters port 445 or configured TCP ports,
  finds or creates an IPv4/TCP aggregate flow in a 16-bucket/16-flow hlist,
  attaches continuation payloads as skb shared-info fragments, and flushes on a
  short payload or `max_gro`. Failed eligibility flushes every pending flow and
  returns zero for ordinary CPU RX processing. A successful return leaves the
  descriptor raw-buffer word owned by GRO state.
- The append decision calls `search_gro_flow(hash, exact_flow)`, but that child
  tests only whether the hash exists and the parent then dereferences the exact
  flow. Preserve this unguarded hash-collision edge rather than inventing a
  defensive behavior.
- `can_tcp_gro @ 0x10a9c`: complete and recorded in
  `functions/0x10a9c-can_tcp_gro.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It returns only an eligibility Boolean. It checks an unresolved reserved-memory
  virtual threshold, raw TCP mask `0x2f00`, new-flow payload `> 0x54f`, and on
  an existing flow requires ingress/IP/L4 tuple equality plus an exact raw
  sequence/ACK arithmetic expression. It has no local writes or locking. If
  the flow count is above 15 and caller passes null, it dereferences null in the
  existing-flow path; preserve that binary behavior.
- `is_l4port_supported @ 0x10930`: complete and recorded in
  `functions/0x10930-is_l4port_supported.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It chooses source or destination port list from the low byte of its selector,
  locks with a specialized BH-disabling helper, traverses list nodes at entry
  `+0x8` looking for a 16-bit port at entry `+0x0`, then releases the global
  lock byte with store-release and restores BH state. Its only callers are the
  two GRO port checks.
- `search_gro_flow @ 0x10e5c`: complete and recorded in
  `functions/0x10e5c-search_gro_flow.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It computes `(1640531527 * hash) >> 28`, walks that bucket, and returns true
  for any matching flow hash at allocation `+0x8`. It is deliberately hash-only
  and has no lock or tuple comparison; `pp_net_tcp_gro` owns the collision edge.
- `pp_tcp_gro_flush @ 0x10c24`: complete and recorded in
  `functions/0x10c24-pp_tcp_gro_flush.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It updates saved inner/optional outer IPv4 length/checksum only when the
  aggregate has fragments, then sends a port-selected skb to the Wi-Fi trap
  callback when available or unconditionally to the switch callback. It does
  not free the skb/flow; the receiver callback takes the aggregate skb.
- `pp_tcp_gro_flush_all @ 0x10dcc`: complete and recorded in
  `functions/0x10dcc-pp_tcp_gro_flush_all.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It sweeps all 16 GRO buckets, flushes each aggregate, unlinks its hlist node,
  decrements `g_cur_flows`, and frees only the flow allocation. CPU RX invokes
  it before ordinary delivery and CPU source-0 NAPI invokes it at poll end.
- `net_gro_init @ 0x1150c`: complete and recorded in
  `functions/0x1150c-net_gro_init.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It is only a callback publication store:
  `pp_smb_test_config = lower_net_smb_test_config`. It does not initialize or
  clear the GRO table and has no error or synchronization path.
- `lower_net_smb_test_config @ 0x10778`: complete and recorded in
  `functions/0x10778-lower_net_smb_test_config.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- Nonzero input sets `net_gro_en=3`, logs, stores 1 to the 16-bit threshold, and
  returns 1. Zero clears GRO/SMB state, logs, sets an affinity hint for
  `g_idm_irq[0]` using the raw `cpu_bit_bitmap` expression, stores 2, and
  returns 2. It is published but not directly called inside the module.
- `__fswab32_0 @ 0x10750`: complete and recorded in
  `functions/0x10750-fswab32_0.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It is the exact two-instruction ARM64 32-bit byte-reversal helper, with no
  state. GRO uses it for network-order fields and debug display.
- `sub_10738 @ 0x10738`: complete and recorded in
  `functions/0x10738-sub_10738.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It has no direct IDB xrefs. Its literal behavior is an `ICC_PMR_EL1` read,
  temporary low-word XOR-`0xe0` write, immediate restore, `DSB SY`, and a
  fall-through return through `__fswab32_0(TPIDR_EL2 low word)`. Purpose remains
  unknown; do not assign it a generic interrupt API name.
- `__raw_spin_lock_bh.constprop.13 @ 0x10808`: complete and recorded in
  `functions/0x10808-raw_spin_lock_bh_constprop_13.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It increments current context word `SP_EL0 + 0x10` by `0x200`, acquires the
  32-bit GRO port-list lock with LDAXR/STXR or queued slowpath, and is paired
  with caller-side low-byte store-release plus `__local_bh_enable_ip`. The
  current-context field name remains inferred.
- `add_supported_l4port @ 0x10864`: complete and recorded in
  `functions/0x10864-add_supported_l4port.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It allocates/zeros a 24-byte entry from the shared GRO state cache, writes a
  port at `+0x0`, and appends node `+0x8` to source/destination circular list
  tail under the specialized lock. It has no deduplication; success returns 1,
  allocation failure logs an unresolved message and returns -1. No direct IDB
  xrefs establish its external/indirect registration boundary.
- `remove_supported_l4port @ 0x109d0`: complete and recorded in
  `functions/0x109d0-remove_supported_l4port.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It locks selected source/destination list, unlinks/frees only its first
  matching port entry after exact `0xDEAD...0100`/`0xDEAD...0122` node poisoning,
  releases BH state, and always returns 1. It has no direct IDB xrefs.
- `net_gso_init @ 0x0f9bc`: complete and recorded in
  `functions/0x0f9bc-net_gso_init.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It enables the upload driver, creates `/proc/upload_ctl/upload` with only
  ratelimited failure logs/no rollback, and always publishes `net_upload_fun`
  through `upload_hook`. The proc operations ABI and hook contract remain open.
- `net_upload_fun @ 0x0ea40`: complete and recorded in
  `functions/0x0ea40-net_upload_fun.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It locks `net_lock_tx` in a BH-disabled exclusive region, makes `upload_count`
  transitions call enable only on 0->1 and disable whenever a zero-input leaves
  the count at zero, then releases the lock. It is called by proc write and
  published through `upload_hook`; known caller ignores its return register.
- `upload_write_proc @ 0x0eb50`: complete and recorded in
  `functions/0x0eb50-upload_write_proc.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- The proc dispatch callback validates/copies a fixed eight bytes regardless of
  write count, then parses an un-NUL-terminated local buffer if its first byte
  is nonzero and calls `net_upload_fun` with the low byte. It returns -1 on
  range/copy failure and original count otherwise. Preserve the observed
  short-write and missing-terminator edges.
- `gso_upload_enable @ 0x0e964`: complete and recorded in
  `functions/0x0e964-gso_upload_enable.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It lazily fills 64 nbuf pool slots, initializing raw nbuf fields/buffer data,
  then publishes count/index only after full success. Allocation failure calls
  `gso_upload_disable(1)` while count is still zero, so its loop frees none of
  the partial nbufs: verified vendor leak under the caller's TX lock.
- `gso_upload_disable @ 0x0e89c`: complete and recorded in
  `functions/0x0e89c-gso_upload_disable.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It resets or frees only slots below current published count, clears count only
  in release mode, and leaves `gso_buf_idx` unchanged. Null slots skip header
  reset. It explains the partial-enable leak above.
- `cpu_net_alloc_nbuf @ 0x0afd8`: complete and recorded in
  `functions/0x0afd8-cpu_net_alloc_nbuf.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It is an unchecked no-argument wrapper over `cpu_net_ops + 0x28`, used by all
  observed GSO allocation paths; nbuf ownership remains a backend contract.
- `cpu_net_free_nbuf @ 0x0b4b0`: complete and recorded in
  `functions/0x0b4b0-cpu_net_free_nbuf.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It frees only when nbuf byte `+0x2c` bit 1 is clear, passing nbuf `+0x10 - 64`
  to IDM ops `+0x30`; set bit means increment a non-release counter. GSO release
  mode clears the bit first, establishing the pool's actual free path.
- `net_gso_upload_send @ 0x0e634`: complete and recorded in
  `functions/0x0e634-net_gso_upload_send.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It configures a CPU TX descriptor from nbuf/skb, encodes payload/segment GSO
  fields with ARM zero-divisor UDIV behavior, barriers, then hands off to
  `cpu_net_nb_desc_tx`; descriptor exhaustion uses gated nbuf free and returns
  -1. Its direct callers are the two upload segmenters.
- `net_tcp_gso_tx_upload @ 0x0ec3c`: complete and recorded in
  `functions/0x0ec3c-net_tcp_gso_tx_upload.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It derives an upload segment template from raw skb headers, reuses/allocates
  an nbuf, copies only header bytes, adjusts IPv4/IPv6/nested checksums, then
  submits it. Pool slot state is updated before the null test, and a stale
  larger header length can cause null dereference before failure. Payload
  association remains delegated to descriptor helpers.
- `net_tcp_gso_tx_upload1 @ 0x0ef38`: complete and recorded in
  `functions/0x0ef38-net_tcp_gso_tx_upload1.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It is the alternate upload software segmenter: allocates fresh nbufs, copies
  headers, advances TCP sequence for every min(payload, GSO-size) segment,
  rewrites IPv4/IPv6/nested checksums, and submits each segment. It returns 0
  for zero/full payload processing and -1 on alloc/send failure; it does not use
  the published pool.
- `net_tcp_gso_tx @ 0x0f258`: complete and recorded in
  `functions/0x0f258-net_tcp_gso_tx.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It validates header/linear layout after clearing original TCP PSH, segments
  across linear and shared-info fragment sources, builds CPU TX nbufs, chooses
  hardware/software checksum metadata, and sends through owner-ring handoff.
  Source-fragment exhaustion logs, submits a potentially truncated final nbuf,
  and still exits successfully; later failures may leave previous segments sent.
- `net_gso_checksum_upload @ 0x0e7f4`: complete and recorded in
  `functions/0x0e7f4-net_gso_checksum_upload.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It uses full TCP length from IPv4 total length in the pseudo-header. Ordinary
  mode partial-checksums full TCP data; upload template mode partial-checksums
  only TCP header bytes, then both write TCP checksum and recompute IPv4 header.
- `net_gso_ipv6tcp_checksum.constprop.6 @ 0x0e788`: complete and recorded in
  `functions/0x0e788-net_gso_ipv6tcp_checksum.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It clears TCP checksum, partial-checksums only TCP header bytes, then writes
  `csum_ipv6_magic` from IPv6 source/destination/payload/protocol 6. The upload
  segmenters use it only outside their next-header-4 nested IPv4 branch.
- `net_cfg_desc_by_skb @ 0x0df4c`: complete and recorded in
  `functions/0x0df4c-net_cfg_desc_by_skb.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It writes common GSO descriptor defaults, conditionally maps skb byte `+0x108`
  to descriptor byte `+0x0a` based on direction/`lan_up`, and sets descriptor
  byte `+0x1b` bits. It has no ownership or synchronization behavior.
- `cpu_net_nb_desc_tx @ 0x0dfa8`: complete and recorded in
  `functions/0x0dfa8-cpu_net_nb_desc_tx.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It tags `nbuf | 1` into CPU TX owner slot `(descriptor - base) >> 5`, increments
  queue pending, and calls CPU-net ops `+0x70(queue_id, 1)`. It always returns
  zero; ownership transfers to completion reclaim.
- Correction: `cpu_net_ops + 0x20` is `idm_get_cpu_tx_q`; CPU-net init retains
  four IDM TX queue records rather than creating task queues.
- `net_get_next_txdesc @ 0x0ce8c`: complete and recorded in
  `functions/0x0ce8c-net_get_next_txdesc.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It reserves the current 32-byte TX descriptor and advances producer `+0x10`
  modulo depth `+0x20`, but does not increment pending, set an owner, or issue a
  doorbell. At pending >= low-16-bit `g_net_check_threshold`, it opportunistically
  calls `net_check_tx_done_nolock`; if still full, it increments `net_tx_full`
  and returns null. The below-threshold branch has no depth check.
- Reservation rollback is separately performed by `net_set_prev_txdesc` after
  direct backend submit failures.
- `net_set_prev_txdesc @ 0x0aff8`: complete and recorded in
  `functions/0x0aff8-net_set_prev_txdesc.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It rolls producer `+0x10` back modulo depth `+0x20` without altering pending,
  owner state, descriptor data, or backend state. Direct TX callers use it after
  failed submit callbacks. A zero depth / zero producer underflows without a
  guard.
- `cpu_net_nb_tx @ 0x0cf14`: complete and recorded in
  `functions/0x0cf14-cpu_net_nb_tx.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- This exported `ipsec.ko` entry selects `unlock_tq[cpu2unlock_tq[cpu]]`, clears
  every linked nbuf's next pointer, fills/tag-owns descriptors, increments
  pending, and calls ops `+0x70` in 256-entry/final batches. Invalid selector >1
  returns -1; descriptor exhaustion takes gated nbuf free/drop/log path but
  continues and retains overall success status.
- Its batch packet-accounting target is the current/final list nbuf device rather
  than a tracked per-nbuf aggregate, so mixed-device or final-failure lists can
  misattribute packet counts. No local lock or barrier is present.
- `cpu_lowpower_tx @ 0x0d49c`: complete and recorded in
  `functions/0x0d49c-cpu_lowpower_tx.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It gates low-power callback use on non-null callbacks and a nonzero initial
  judge result, optionally calls `(0, 0, skb +0x130, skb +0xa8, 0)`, then performs
  one/two further judge calls. Exact results plus device type determine whether
  descriptor byte `+0x0a` port 64 gets an encoded `(skb_len + 4)` length.
- `low_power_send` and `low_power_up_en_judge` are externally populated by the
  two `regisetr_*` callback stores at `0xafc0` and `0xafcc`; no callback locking
  or lifetime protection is present.
- `regisetr_low_power_send_pkt_handle @ 0x0afc0`: complete and recorded in
  `functions/0x0afc0-regisetr_low_power_send_pkt_handle.md`; source-like C is
  in `recovered/plat_cpu_net.c`.
- It returns and unsafely publishes its supplied five-argument callback pointer,
  including null. `np.ko` imports it; callback lifetime remains external.
- `regisetr_low_power_up_en_judge_handle @ 0x0afcc`: complete and recorded in
  `functions/0x0afcc-regisetr_low_power_up_en_judge_handle.md`; source-like C
  is in `recovered/plat_cpu_net.c`.
- It is the paired unsynchronized callback store/identity return. `np.ko` imports
  it, while `cpu_lowpower_tx` consumes its mutable callback slot.
- `register_omci_oam_handle @ 0x0af68`: complete and recorded in
  `functions/0x0af68-register_omci_oam_handle.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It publishes/returns the OMCI/OAM receive callback without synchronization.
  Both the API and `omci_oam_rx` storage are exported; `np.ko` imports the slot
  directly, so either external path can race callback consumers.
- `regisetr_omci_mic_add_handle @ 0x0af74`: complete and recorded in
  `functions/0x0af74-regisetr_omci_mic_add_handle.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It is an unsynchronized identity-return callback publication. CPU management TX
  invokes the slot only under mode bits `0x600`; no captured core companion
  module imports this API.
- `idm_omci_portid_set @ 0x0af80`: complete and recorded in
  `functions/0x0af80-idm_omci_portid_set.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It is an exported one-instruction `RET` stub. It neither reads ABI inputs nor
  writes `local_omci_port_id`, whose only known module uses are OMCI RX/TX.
- `register_omci_mic_check_handle @ 0x0af84`: complete and recorded in
  `functions/0x0af84-register_omci_mic_check_handle.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It is an unsynchronized identity-return publication for the callback consumed
  by CPU OMCI RX. No captured core companion module imports the API.
- `register_woe_recycle_handle @ 0x0af90`: complete and recorded in
  `functions/0x0af90-register_woe_recycle_handle.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It writes callback slot 0 of a three-slot recycle array. The callback ABI is
  now resolved by `net_check_reorder_rls_nolock`: `(slot, stack_context)` where
  context has a completion-ring base, ring size, release index, and count.
- `register_woe1_recycle_handle @ 0x0af9c`: complete and recorded in
  `functions/0x0af9c-register_woe1_recycle_handle.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It is the identical unsynchronized callback store for exact array slot 1.
- `register_woe2_recycle_handle @ 0x0afa8`: complete and recorded in
  `functions/0x0afa8-register_woe2_recycle_handle.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It completes the setter family at exact array slot 2 (`idm_recycle_cb + 0x10`).
  Slot 2 is processed only when `rls_ring_num_max` is three, which CPU-net init
  selects for CPU 133/129; external callback providers/lifetimes remain unknown.
- `register_wlan_to_essid_handle @ 0x0afb4`: complete and recorded in
  `functions/0x0afb4-register_wlan_to_essid_handle.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It stores/returns a raw callback pointer in the private global
  `idm_wlanname_to_essid`. `np.ko` `tm_initial` registers its
  `aclDevNameToWlanIDMMap` provider, which presents a candidate one-input,
  three-byte-output, integer-status ABI. No other `plat_132` slot access is
  resolved, so do not assign the callback a definitive type or consumer.
- `get_next_rxdesc @ 0x0b010`: complete and recorded in
  `functions/0x0b010-get_next_rxdesc.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It returns and prefetches `queue->descriptor_base +
  (queue->producer << ((uNPPT_IDM_DESC_MODE + 5) & 31))`, then increments
  producer `+0x8` and resets it at `>= depth +0xc`. The shift mask is needed to
  reproduce ARM64 32-bit register semantics. There is no local synchronization
  or bounds validation. Only CPU and IDM RX invoke it, using 16-byte IDM queue
  records initialized with base, zero producer, and configured depth.
- `net_check_reorder_rls_nolock @ 0x0b050`: complete and recorded in
  `functions/0x0b050-net_check_reorder_rls_nolock.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It reads up to three release counts through IDM ops `+0x58`, clamps each to
  `0xfff`, dispatches a non-null recycle callback with a stack-local copy of
  completion-ring state, then advances the local release index/counter only for
  dispatched slots. It always submits all three counts through ops `+0x60`.
  Direct callers hold `idm_lock_tx`; callback registration does not. The count
  array has three elements but `rls_ring_num_max` is not locally bounded.
- `idm_init` initializes three adjacent completion-ring bases; CPU-net init
  initializes the matching normal/jumbo/extra ring-size array.
- `dev_kfree_skb_any @ 0x0b1a0`: complete and recorded in
  `functions/0x0b1a0-dev_kfree_skb_any.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It is a six-instruction local wrapper for imported
  `__dev_kfree_skb_any(skb, 1)`. The literal reason value is verified but has no
  vendor-ABI semantic label. It is used by TX drop and untagged completion
  reclaim paths, and transfers ownership directly to the imported helper.
- `napi_complete @ 0x0b1b8`: complete and recorded in
  `functions/0x0b1b8-napi_complete.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It is a six-instruction local wrapper for imported
  `napi_complete_done(napi, 0)`. Four NAPI poll completion paths invoke it;
  its residual return register is not used. The zero work-done value is exact,
  but its vendor-kernel lifecycle meaning is not inferred.
- `dump_net_condition_set @ 0x0b2ec`: complete and recorded in
  `functions/0x0b2ec-dump_net_condition_set.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It unconditionally stores the low print-type byte, then only configures a
  selected condition for indices 0 or 1. Each 24-byte condition stores swapped
  64-bit mask/value and a raw 32-bit shift used by `dump_net_check` as a packet
  byte offset. Mode 0 accepts all, 1 requires condition 0, 2 negates it, 3 ORs
  both conditions, and 4 requires both. `np.ko` config-store code imports the
  API and parses its five documented values from `print_config` input.
- It has no synchronization with RX/TX predicate readers. Preserve its observed
  logging bug: condition-1 status prints condition-0's shift.
- `cpu_timer_unlock @ 0x0b700`: complete and recorded in
  `functions/0x0b700-cpu_timer_unlock.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It derives one of two 40-byte timer indices, runs nolock completion reclaim on
  the matching unlock queue, updates that canonical timer's expiry to
  `jiffies + 1`, and requeues it on `ipsec_tx_cpu`. It takes no lock or null
  checks. CPU-net initialization aliases both unlock slots to the queue from IDM
  TX index 3.
- `do_raw_spin_lock @ 0x0b768`: complete and recorded in
  `functions/0x0b768-do_raw_spin_lock.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It prefetches the lock, uses `LDAXR`/`STXR` to acquire zero-valued words with
  acquire semantics, retries exclusive-store loss, and invokes
  `queued_spin_lock_slowpath(lock, observed, 0, 1)` for contention. Nine TX,
  timer, and NAPI call sites pair it with low-byte store-release unlocks.
- `dump_net_check @ 0x0bae4`: complete and recorded in
  `functions/0x0bae4-dump_net_check.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It returns zero for debug-filter acceptance and -1 for rejection/bounds
  failure. Modes 0/other accept without a read; 1 requires condition 0, 2
  negates it, 3 ORs conditions, and 4 requires both. Each match is a 64-bit
  packet load at the configured raw shift, masked and compared to value. No lock
  protects concurrent condition updates.
- `dump_net_data @ 0x0bc30`: complete and recorded in
  `functions/0x0bc30-dump_net_data.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It emits at most 192 bytes as `%.2x ` printk output, inserts a prefixed newline
  after every 16 bytes, and always emits a final newline. It has no filter,
  pointer check, state mutation, or ownership behavior; 14 RX/TX debug paths
  call it after their own gates.
- `dump_net_desc @ 0x0bcb8`: complete and recorded in
  `functions/0x0bcb8-dump_net_desc.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It is a read-only descriptor logger: it prints two fixed raw-word lines, then
  either L3 flags or trap reason/detail/SSID bytes based on its nonzero format
  selector. Five RX/trap debug paths use it; raw field semantics remain open.
- `idm_set_wifi_trap_info @ 0x0bd8c`: complete and recorded in
  `functions/0x0bd8c-idm_set_wifi_trap_info.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It writes exactly 36 bytes of trap metadata: queue, raw descriptor extractions
  from bytes 6/7, queue-equals-15 Boolean, and an exact copy of descriptor bytes
  8 through 23. CPU/IDM RX callers provide 40-byte stack ranges, while the GRO
  caller explicitly zeroes only the 36 bytes written by this helper; no
  in-module consumer reads a trailing field.
- Reasons `0x62`/`0x63` and `0x65` select separate debug-count/budget paths;
  accepted debug dumps decrement the corresponding budget. It also increments a
  raw counter for descriptor byte 7 bit 5 and returns the full raw byte 7, which
  all callers ignore. The decrement writes a pre-filter budget snapshot, so it
  can overwrite a concurrent debug-budget update; no lock or ownership
  transition is present.
- `cpu_dev_stat @ 0x0bf3c`: complete and recorded in
  `functions/0x0bf3c-cpu_dev_stat.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It returns null for a null device or the exact raw pointer `(void *)-0x880`;
  otherwise it returns `device + 0x890` without dereferencing. Its 31 direct
  call sites treat the result as a netdev statistics record, but the original
  field name/type and sentinel provenance remain unknown.
- `cpu_eth_get_stats @ 0x0bf58`: complete and recorded in
  `functions/0x0bf58-cpu_eth_get_stats.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It forwards a device pointer to `cpu_dev_stat` with no local policy and returns
  the result unchanged. Two static operation-table entries at `0x1dd18` and
  `0x1df38` reference it; the original table field name is unconfirmed.
- `cpu_net_free_buf @ 0x0c3cc`: complete and recorded in
  `functions/0x0c3cc-cpu_net_free_buf.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It forwards live `(buffer, pool)` ABI arguments through `cpu_net_ops + 0x30`
  with no guard or local ownership policy. `idm_skb_stack_push` is its sole
  caller; the wrapper's residual return is not semantically consumed.
- `__raw_spin_lock_irqsave @ 0x0ba8c`: complete and recorded in
  `functions/0x0ba8c-raw_spin_lock_irqsave.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It returns the original DAIF, conditionally masks local IRQs through
  `DAIFSet #2`, then acquires the supplied raw lock with `LDAXR`/`STXR` or
  `queued_spin_lock_slowpath(lock, observed, 0, 1)`. Five TX call sites restore
  the returned DAIF after byte-release unlock.
- `arch_local_irq_restore @ 0x0af5c`: complete and recorded in
  `functions/0x0af5c-arch_local_irq_restore.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It writes the supplied full 64-bit snapshot to `DAIF`, so it restores all
  represented mask bits after caller-side byte-release unlock. Its residual
  input-valued return register is ignored by all three direct TX callers.
- `arch_local_irq_save @ 0x0fb7c`: complete and recorded in
  `functions/0x0fb7c-arch_local_irq_save.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It returns the full DAIF and conditionally executes `DAIFSet #2` only when the
  IRQ mask bit was clear. Two FIFO allocation/free helpers pair it with the
  separate restore wrapper at `0xfb94`.
- `arch_local_irq_restore_0 @ 0x0fb94`: complete and recorded in
  `functions/0x0fb94-arch_local_irq_restore_0.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It is a separate three-instruction full-DAIF writer with four direct FIFO
  callers, despite having the same raw semantics as `arch_local_irq_restore` at
  `0xaf5c`.
- `__my_cpu_offset @ 0x0fba0`: complete and recorded in
  `functions/0x0fba0-my_cpu_offset.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It only returns raw `TPIDR_EL1`; nine skb-stack, allocation, and FIFO callers
  add it to CPU-local state regions. Per-CPU offset/base semantics remain a
  strong inference rather than a vendor type fact.
- `do_raw_spin_lock_flags.isra.2 @ 0x0fba8`: complete and recorded in
  `functions/0x0fba8-do_raw_spin_lock_flags_isra_2.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It is the IRQ-neutral FIFO lock acquire helper: `LDAXR`/`STXR` on zero or
  `queued_spin_lock_slowpath(lock, observed, 0, 1)` on contention. The two FIFO
  callers provide IRQ save/restore and low-byte release stores.
- `do_raw_spin_lock_0 @ 0x0fc28`: complete and recorded in
  `functions/0x0fc28-do_raw_spin_lock_0.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It is a distinct, machine-code-equivalent acquire helper for the FIFO path
  selected by a nonzero raw `SP_EL0 + 0x10` predicate; this path has no IRQ
  save/restore calls.
- `_buf_fifo_free_data @ 0x0fbe4`: complete and recorded in
  `functions/0x0fbe4-buf_fifo_free_data.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- Selection zero calls `kfree_skb_without_data`, one calls
  `kmem_cache_free(kmem_buf_cache, object)`, and all other values call
  `__dev_kfree_skb_any(object, 1)`. FIFO callers own selection/ownership policy.
- `buf_fifo_free_data @ 0x0fc64`: complete and recorded in
  `functions/0x0fc64-buf_fifo_free_data.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- A 32-byte FIFO record has producer/consumer/mask at offsets 0/4/8, lock at
  0x10, and entries at 0x18. High context stages 32 entries before a locked
  batch commit; low context saves DAIF and commits one. No-room paths call the
  selection-specific release helper.
- `buf_fifo_alloc_data @ 0x1003c`: complete and recorded in
  `functions/0x1003c-buf_fifo_alloc_data.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It consumes the same ring. High context refills up to 32 entries into staging
  at `+0x100` and pops locally; low context dequeues one under DAIF save/restore.
  Empty or null slots return null with separate raw counters.
- `idm_skb_stack_pop @ 0x1029c`: complete and recorded in
  `functions/0x1029c-idm_skb_stack_pop.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- Its selector low bit chooses per-CPU `wifi0/1_free_data` and FIFO 2/3. It
  recycles a returned skb, preserves only skb word `+0x114` bit 0, then frees
  short candidates and increments one of two raw counters. There are no direct
  IDA callers.
- `net_alloc_skb @ 0x10354`: complete and recorded in
  `functions/0x10354-net_alloc_skb.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It adds TPIDR_EL1 to `skb_free_data` and forwards directly to FIFO selection
  zero; its sole direct caller is IDM RX.
- `net_alloc_kmem @ 0x10380`: complete and recorded in
  `functions/0x10380-net_alloc_kmem.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It adds TPIDR_EL1 to `kmem_free_data` and forwards directly to FIFO selection
  one; its sole direct caller is `idm_alloc_buf`.
- `net_free_kmem @ 0x103ac`: complete and recorded in
  `functions/0x103ac-net_free_kmem.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It forwards its object argument through `kmem_free_data + TPIDR_EL1` and FIFO
  selection one, returning the raw helper counter to `idm_free_buf`.
- `idm_skb_stack_wifi_push @ 0x103dc`: complete and recorded in
  `functions/0x103dc-idm_skb_stack_wifi_push.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It gives skb word `+0x114` bit 17 priority over bit 18, selecting stack FIFO
  2 or 3, otherwise frees with reason 1. Its sole caller is IDM TX completion
  reclaim.
- `_idm_skb_stack_push @ 0x0fec4`: complete and recorded in
  `functions/0x0fec4-idm_skb_stack_push.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It rejects four raw skb states into fixed-reason free, otherwise routes a
  zero selector to `wifi0_free_data`/FIFO 2 and any nonzero byte to
  `wifi1_free_data`/FIFO 3. It always returns zero.
- Both this helper and `idm_skb_stack_push` are exported through their respective
  `__ksymtab` entries; only the former has an in-module direct caller.
- `idm_skb_stack_push @ 0x0ffd8`: complete and recorded in
  `functions/0x0ffd8-idm_skb_stack_push.md`; source-like C is in
  `recovered/plat_cpu_tx.c`.
- It requires skb word `+0x114` bits 16 and 0, otherwise frees the skb with
  reason 1. On success it returns `skb + 0x128` to the CPU-net buffer-free
  operation with pool 0, then stages the skb in `skb_free_data + TPIDR_EL1` for
  FIFO 0. It has no in-module code xrefs but is exported. Its machine code does
  not normalize X0 after either callee, so the semantic void signature is a
  strong inference rather than a proven external ABI.
- `nppt_smac_init @ 0x129c8`: complete and recorded in
  `functions/0x129c8-nppt_smac_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- It initializes seven PHY-status/callback slots, uses CPU 129 to select MAC
  max index 2 and all other CPU types to select 3, configures each SMAC block
  with verified raw values, selects XMAC work-mode pairs from the external PHY
  type, starts the PHY polling worker, and always returns zero. On the CPU-133
  runtime it configured MAC 0 through 3 and selected XMAC modes 5 and 4.
- `smac_thread_init @ 0x128ec`: complete and recorded in
  `functions/0x128ec-smac_thread_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- It creates `smac_check_phy_task_thread` with null data, node `-1`, and name
  `smac_check_phy_task`; error pointers only log failure, while success wakes the
  returned task then logs success. It stores no task pointer and has no explicit
  normalized return value.
- `smac_check_phy_task_thread @ 0x12890`: complete and recorded in
  `functions/0x12890-smac_check_phy_task_thread.md`; source-like C is in
  `recovered/plat_smac.c`.
- It ignores its null thread argument, stops only when the low byte of
  `kthread_should_stop()` is nonzero, calls `check_phy` for MAC 0 through 6 in
  fixed order, then sleeps interruptibly for 100 ms. It has no direct state.
- `check_phy @ 0x126e4`: complete and recorded in
  `functions/0x126e4-check_phy.md`; source-like C is in
  `recovered/plat_smac.c`.
- It calls each installed PHY callback only while `check_phy_en != 1`, compares
  its raw status with a seven-slot cache, and acts only on changes. Status `-1`
  disables the MAC; other statuses use low byte as speed and bit 10 as duplex,
  then configure normal SMAC or guarded XMAC paths and enable the MAC. MAC 4/5
  map to XMAC slots 0/1; all other indexes use normal SMAC configuration.
- `xmac_init @ 0x18460`: complete and recorded in
  `functions/0x18460-xmac_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- Raw PHY type 9 skips PHY setup; every other type runs five ordered PHY setup
  helpers. Independently enabled XMAC0 and XMAC1 mode setup results are ORed and
  returned, though `nppt_smac_init` ignores them. No local hardware writes or
  cleanup occur.
- `xmac_init_by_work_mode @ 0x17da0`: complete and recorded in
  `functions/0x17da0-xmac_init_by_work_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It truncates the selector to a byte, always initializes XPCS and resets XMAC,
  dispatches modes 0 through 9 to verified setters, and runs a shared
  duplex/delay/SOPC/TX-RX-enable tail even when a setter fails. It updates the
  work-mode cache only on success; invalid modes return `-1` after pre-switch
  work only.
- `xmac_10gbase_r_conf @ 0x1718c`: complete and recorded in
  `functions/0x1718c-xmac_10gbase_r_conf.md`; source-like C is in
  `recovered/plat_smac.c`.
- Valid selectors 0 through 4 disable TX/RX. CPU 132 selectors 0/1 configure
  XPCS before MAC/SerDes and OR XPCS plus SerDes status; all other selectors
  configure SerDes before XPCS and return only the XPCS status. Both paths write
  work mode zero even on error; CPU 133 selectors 0/1 additionally enable
  bypass.
- `xmac_5gbase_r_conf @ 0x17280`: complete and recorded in
  `functions/0x17280-xmac_5gbase_r_conf.md`; source-like C is in
  `recovered/plat_smac.c`.
- It has the same selector and CPU branch structure as the 10G setter, but uses
  XPCS 5GBASE-R, speed value 5, SerDes mode 2, and writes work mode one on all
  valid paths, including error returns.
- `xmac_1gbase_x_conf @ 0x1781c`: complete and recorded in
  `functions/0x1781c-xmac_1gbase_x_conf.md`; source-like C is in
  `recovered/plat_smac.c`.
- It uses XPCS arguments `(3, 1)`, MAC speed three, and SerDes mode seven. CPU
  132 XMAC0/1 OR XPCS/SerDes status; all other valid paths return only XPCS.
  CPU 133 or 129 enables bypass for XMAC0/1, and valid paths write mode two.
- `xmac_sgmii_conf @ 0x16ee4`: complete and recorded in
  `functions/0x16ee4-xmac_sgmii_conf.md`; source-like C is in
  `recovered/plat_smac.c`.
- It forwards two raw PCS values and a byte auto-negotiation value. CPU 132
  XMAC0/1 ORs PCS mode, auto-negotiation, and SerDes status; all other valid
  paths omit only the SerDes status. CPU 133 or 129 enables bypass for XMAC0/1,
  and every valid path writes mode three.
- `xmac_2pt5gbase_x_conf @ 0x17378`: complete and recorded in
  `functions/0x17378-xmac_2pt5gbase_x_conf.md`; source-like C is in
  `recovered/plat_smac.c`.
- It uses XPCS 2.5GBASE-X, speed six, and SerDes mode five. CPU 132 XMAC0/1 OR
  XPCS/SerDes status; all other valid paths return only XPCS. CPU 133 or 129
  enables bypass for XMAC0/1, and valid paths write mode four.
- `xmac_10g_usxgmii_auto_conf @ 0x17484`: complete and recorded in
  `functions/0x17484-xmac_10g_usxgmii_auto_conf.md`; source-like C is in
  `recovered/plat_smac.c`.
- It uses USXGMII mode zero, auto-negotiation value one, MAC speed zero, and
  SerDes mode one. CPU 132 XMAC0/1 ORs all three setup results; other valid
  paths omit only SerDes status. Only CPU 133 enables bypass for XMAC0/1, and
  valid paths write mode five.
- `xmac_5g_usxgmii_auto_conf @ 0x175b0`: complete and recorded in
  `functions/0x175b0-xmac_5g_usxgmii_auto_conf.md`; source-like C is in
  `recovered/plat_smac.c`.
- It uses USXGMII mode one, auto-negotiation one, MAC speed five, and SerDes
  mode three. CPU 132 XMAC0/1 ORs all three setup results; other valid paths
  omit only SerDes status. Only CPU 133 enables bypass for XMAC0/1, and valid
  paths write mode six.
- `xmac_2pt5g_usxgmii_auto_conf @ 0x176dc`: complete and recorded in
  `functions/0x176dc-xmac_2pt5g_usxgmii_auto_conf.md`; source-like C is in
  `recovered/plat_smac.c`.
- It uses USXGMII mode two, auto-negotiation one, MAC speed six, and SerDes mode
  four. CPU 132 XMAC0/1 ORs all three setup results; other valid paths omit only
  SerDes status. CPU 133 or 129 enables bypass for XMAC0/1, and valid paths
  write mode seven.
- `xmac_hsgmii_conf @ 0x17938`: complete and recorded in
  `functions/0x17938-xmac_hsgmii_conf.md`; source-like C is in
  `recovered/plat_smac.c`.
- It forwards a byte variant to HSGMII PCS, uses MAC speed two and SerDes mode
  five, and follows the CPU-132 status-OR pattern. CPU 133 or 129 enables bypass
  for XMAC0/1. Valid paths cache mode eight regardless of HSGMII variant.
- `xmac_mode_set @ 0x17bd8`: complete and recorded in
  `functions/0x17bd8-xmac_mode_set.md`; source-like C is in
  `recovered/plat_smac.c`.
- It validates outer PCS modes, maps an input-speed byte through the verified
  six-byte table `{7,4,3,2,5,0}`, dispatches PCS modes 0 through 9, and invokes
  speed selection after all modes except HSGMII. Its three in-module callers are
  in `phy_zxic051_check`, establishing the PHY-to-XMAC mode boundary.
- `phy_zxic051_check @ 0x1c0c0`: complete and recorded in
  `functions/0x1c0c0-phy_zxic051_check.md`; source-like C is in
  `recovered/plat_smac.c`.
- It maps an incoming PHY byte to XMAC slot 0/1, queries PHY 4/5, tracks a mode
  history and two recovery counters, and replays `xmac_mode_set` at the shared
  threshold on CPU 133/129. It returns `-1` until PHY/global links and XMAC speed
  reconcile, then returns low-byte speed plus duplex bit 10.
- `phy_051_set_xmac_speed @ 0x1c00c`: complete and recorded in
  `functions/0x1c00c-phy_051_set_xmac_speed.md`; source-like C is in
  `recovered/plat_smac.c`.
- It rejects speeds above six. Raw PCS mode three maps the speed through
  `xmac_switch_uni_speed_to_xmac_speed`, writes XMAC speed-select, and logs;
  raw mode six invokes the one-argument SGMII auto-mode processor; all other
  modes are no-ops. The IDA signature now records its verified void return.
- `phy_051_set_xmac_work_mode @ 0x1bfa4`: complete and recorded in
  `functions/0x1bfa4-phy_051_set_xmac_work_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It returns the 2.5GBASE-X configuration status for raw mode three; for raw
  mode six it returns `xmac_sgmii_conf(xmac, 1, 3, 1)` after unconditionally
  writing speed-select one; all other modes return zero. No external xref is
  present in the analyzed module image.
- `xmac_switch_uni_speed_to_xmac_speed @ 0x17fdc`: complete and recorded in
  `functions/0x17fdc-xmac_switch_uni_speed_to_xmac_speed.md`; source-like C is
  in `recovered/plat_smac.c`.
- It writes speed-select values for UNI inputs one through six as
  `{7, 4, 3, mode_8_or_9 ? 2 : 6, 5, 0}`. Other inputs leave the caller's output
  word untouched; all six direct callers use the output word rather than a
  return value.
- `xmac_set_speed_sel @ 0x1670c`: complete and recorded in
  `functions/0x1670c-xmac_set_speed_sel.md`; source-like C is in
  `recovered/plat_smac.c`.
- It selects raw windows for XMAC two/three and NPPT-relative windows for all
  other byte selectors, then preserves the low 29 bits while replacing bits
  `31:29` with the low three bits of the speed value. All nine callers discard
  its return register.
- `xmac_speed_process_in_sgmii_auto_mode @ 0x18058`: complete and recorded in
  `functions/0x18058-xmac_speed_process_in_sgmii_auto_mode.md`; source-like C
  is in `recovered/plat_smac.c`.
- It requires a nonzero auto flag, work mode three, a successful PCS query, and
  `auto_status == 1`, then maps the reported speed and writes speed-select.
  The exact PCS reader ABI was verified and annotated at `0x19bdc` as
  `(xmac, &speed, &duplex, &auto_status)`.
- `xmac_speed_process_in_usxgmii_auto_mode @ 0x18530`: complete and recorded in
  `functions/0x18530-xmac_speed_process_in_usxgmii_auto_mode.md`; source-like
  C is in `recovered/plat_smac.c`.
- It is the mode-five-through-seven USXGMII counterpart, using the PCS-reader
  ABI verified and annotated at `0x19910` as
  `(xmac, &speed, &duplex, &auto_status)`. It is unreferenced in this module
  image.
- `xmac_speed_process @ 0x1860c`: complete and recorded in
  `functions/0x1860c-xmac_speed_process.md`; source-like C is in
  `recovered/plat_smac.c`.
- It inlines the mode-three SGMII and raw mode-five-through-seven USXGMII
  query/map/write paths behind the auto flag; every other work mode has no
  effect. `phy_zxic051_check` calls it during global-link-up reconciliation.
- `xmac_config_speed_duplex @ 0x18130`: complete and recorded in
  `functions/0x18130-xmac_config_speed_duplex.md`; source-like C is in
  `recovered/plat_smac.c`.
- It does a speed-only fast path when duplex matches. On a duplex change it
  optionally disables CPU-133 SOPC auto-gate only when the saved value equals
  one, resets/configures XMAC, then re-enables after a second CPU check.
- `xmac_get_duplex_mode @ 0x16c84`: complete and recorded in
  `functions/0x16c84-xmac_get_duplex_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reads bit 24 from a selector-specific register and maps set to output zero,
  clear to output one. Its only caller uses the output to choose speed-only or
  full duplex reconfiguration.
- `xmac_set_duplex_mode @ 0x16bfc`: complete and recorded in
  `functions/0x16bfc-xmac_set_duplex_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It performs the getter-register RMW inverse: zero duplex sets bit 24; any
  nonzero duplex clears it. All three callers discard the return register.
- `xmac_set_sopc_duplex_mode @ 0x17d38`: complete and recorded in
  `functions/0x17d38-xmac_set_sopc_duplex_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It rejects byte selectors above four, then updates bit `xmac + 4` in
  `nppt_base + 0x343f0`: exactly one clears the bit; every other duplex value
  sets it.
- `xmac_sopc_send_enable @ 0x17f24`: complete and recorded in
  `functions/0x17f24-xmac_sopc_send_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It logs and polls `xmac + 0xa9` until bit zero is set, delays, then writes one
  at `xmac + 0xb0`; no timeout exists. It is called only after full XMAC
  speed/duplex reconfiguration.
- `xamc_init_conf_by_speed @ 0x16d6c`: complete and recorded in
  `functions/0x16d6c-xamc_init_conf_by_speed.md`; source-like C is in
  `recovered/plat_smac.c`.
- It programs selector-2/3 raw windows or other NPPT-relative windows with
  different offsets, calls speed-select plus duplex-one setup, writes three
  fixed words, and sets bit 9 through a final RMW. Nineteen recovered mode and
  runtime configuration paths call it.
- `xmac_reset @ 0x169e4`: complete and recorded in
  `functions/0x169e4-xmac_reset.md`; source-like C is in
  `recovered/plat_smac.c`.
- It maps XMAC zero to reset mask `0x400` and every nonzero low byte to `0x800`,
  then delegates to `smac_reset`.
- `smac_reset @ 0x12358`: complete and recorded in
  `functions/0x12358-smac_reset.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears mask bits at `nppt_base + 0x2c0004`, delays, then writes the original
  value ORed with the mask, guaranteeing masked bits are set afterward.
- `sopc_send_enable @ 0x11fe0`: complete and recorded in
  `functions/0x11fe0-sopc_send_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- Normal SMACs poll at most ten times and always force a final send-enable
  write. MAC6 uses up to ten attempts with 100 delays before each check and
  writes only after readiness.
- `nppt_smac_config_speed_duplex @ 0x123ec`: complete and recorded in
  `functions/0x123ec-nppt_smac_config_speed_duplex.md`; source-like C is in
  `recovered/plat_smac.c`.
- It derives a MAC/RGMII config from byte speed/duplex, updates SOPC state even
  without reset, and on bit-13 duplex change performs CPU-133 gate handling,
  SMAC reset, normal/MAC6 reinitialization, and send-enable.
- `sub_11FCC @ 0x11fcc`: complete and recorded in
  `functions/0x11fcc-sub_11FCC.md`; source-like C is in
  `recovered/plat_smac.c`.
- It toggles/restores `ICC_PMR_EL1`, issues `DSB SY`, then falls through into
  `sopc_send_enable` with the saved PMR low byte. No in-module xref targets it.
- `nppt_smac_disable @ 0x12178`: complete and recorded in
  `functions/0x12178-nppt_smac_disable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears config bits zero/one in normal SMAC or MAC6 RGMII paths, delegates
  XMAC disable for other out-of-range slots, and only normal slots update the
  post-disable SOPC bit.
- `nppt_smac_enable @ 0x12250`: complete and recorded in
  `functions/0x12250-nppt_smac_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It sets config bits zero/one in normal/MAC6 paths or delegates XMAC enable.
  Its normal SOPC update applies only when the MAC equals the current maximum;
  MAC6 uses a separate bit-six update.
- `nppt_smac_set_uni_mode @ 0x1295c`: complete and recorded in
  `functions/0x1295c-nppt_smac_set_uni_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears bits `25:23` in a selector-derived NPPT word and ORs raw mode input
  without validation; initialization passes zero.
- `xmac_tx_rx_enable @ 0x16bbc`: complete and recorded in
  `functions/0x16bbc-xmac_tx_rx_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It preserves raw `W0` around RX enable, then invokes TX enable. It is used by
  normal XMAC initialization and delegated SMAC link-up.
- `xmac_tx_rx_disable @ 0x16bdc`: complete and recorded in
  `functions/0x16bdc-xmac_tx_rx_disable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It preserves raw `W0` around RX disable, then invokes TX disable. It serves
  delegated SMAC link-down handling.
- `xmac_tx_disable @ 0x16a0c`: complete and recorded in
  `functions/0x16a0c-xmac_tx_disable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It uses raw selector values two/three for special addresses; all other values
  use the selector's low 14 bits in the NPPT-relative TX word, then clears bit
  zero by RMW.
- `xmac_rx_disable @ 0x16ad4`: complete and recorded in
  `functions/0x16ad4-xmac_rx_disable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It uses the same raw selector rules as TX disable, targeting special offset
  four or NPPT-relative offset `0x140010`, then clears bit zero by RMW.
- `xmac_get_nppt_glb_link_status @ 0x16984`: complete and recorded in
  `functions/0x16984-xmac_get_nppt_glb_link_status.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reads `nppt_base + 0x84`, shifts by the raw selector's low five bits, and
  writes the normalized selected bit to caller storage for PHY reconciliation.
- `xmac_get_uni_speed_from_xmac @ 0x16cdc`: complete and recorded in
  `functions/0x16cdc-xmac_get_uni_speed_from_xmac.md`; source-like C is in
  `recovered/plat_smac.c`.
- It maps high three speed-select bits through `{6, 7, 4, 3, 2, 5, 4, 1}` by a
  computed branch table and writes the result to the PHY reconciliation output.
- `xmac_tx_enable @ 0x16a70`: complete and recorded in
  `functions/0x16a70-xmac_tx_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It uses raw selector values two/three for special addresses; all other values
  use the selector's low 14 bits in the NPPT-relative TX word, then sets bit
  zero by RMW.
- `xmac_rx_enable @ 0x16b48`: complete and recorded in
  `functions/0x16b48-xmac_rx_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It uses the same raw selector rules as TX enable, targeting special offset
  four or NPPT-relative offset `0x140010`, then sets bit zero by RMW.
- `xmac_set_pcs_for_sgmii_half_duplex @ 0x1874c`: complete and recorded in
  `functions/0x1874c-xmac_set_pcs_for_sgmii_half_duplex.md`; source-like C is
  in `recovered/plat_smac.c`.
- It indexes `sg_xmac_work_mode` and `g_xmac_work_in_auto` with the full input
  selector, then narrows that selector to a PCS byte only after requiring mode
  three and a nonzero auto flag. `configure == 1` passes `(speed, state)` to
  the PCS speed/duplex helper and clears AN enable. Any other configure value
  preserves a currently enabled AN state; otherwise it writes cached mode
  three, state one, link status zero, and enables AN.
- Its two direct callers are the link-down and half-duplex link-up paths in
  `check_phy`. The IDA type at `0x1874c` is now
  `void __fastcall xmac_set_pcs_for_sgmii_half_duplex(unsigned int, unsigned int, unsigned int, unsigned int)`.
- `xpcs_set_speed_duplex_in_sgmii_anto_disale_mode @ 0x19ba4`: complete and
  recorded in `functions/0x19ba4-xpcs_set_speed_duplex_in_sgmii_anto_disale_mode.md`;
  source-like C is in `recovered/plat_smac.c`.
- It narrows its selector to a byte, passes `speed` to the SR-MII speed writer,
  passes `state` to the SR-MII duplex writer, and writes SGMII link status one.
  The sole direct caller is `xmac_set_pcs_for_sgmii_half_duplex`. The IDA type
  at `0x19ba4` is now the recovered three-argument void signature.
- `xpcs_set_sr_mii_ctrl_speed @ 0x18f44`: complete and recorded in
  `functions/0x18f44-xpcs_set_sr_mii_ctrl_speed.md`; source-like C is in
  `recovered/plat_smac.c`.
- It selects a direct window for byte selectors two/three and a
  `xmac0_pcs_base`-relative window otherwise, reads the control word, and for
  speed inputs one through six replaces bits 13, 6, and 5 with
  `{0, 0x2000, 0x40, 0x20, 0x2020, 0x2040}`. Unsupported inputs return after
  the read without a write. The IDA type at `0x18f44` is now the recovered
  two-argument void signature.
- `xpcs_set_sr_mii_ctrl_duplex_mode @ 0x19010`: complete and recorded in
  `functions/0x19010-xpcs_set_sr_mii_ctrl_duplex_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It selects the same SR-MII control window as the speed writer and replaces
  only bit eight with bit zero of the raw state input. The IDA type at `0x19010`
  is now the recovered two-argument void signature.
- `xpcs_set_sr_mii_ctrl_an_enable @ 0x19104`: complete and recorded in
  `functions/0x19104-xpcs_set_sr_mii_ctrl_an_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It selects the same SR-MII control window and replaces only bit 12 with bit
  zero of its raw enable input. Nine direct callers discard its incidental
  pointer return. The IDA type at `0x19104` is now the recovered void signature.
- `xpcs_sr_mii_ctrl_is_an_enable @ 0x19184`: complete and recorded in
  `functions/0x19184-xpcs_sr_mii_ctrl_is_an_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reads the same selector-specific SR-MII control word and returns normalized
  bit 12. Its sole direct caller is the recovered SGMII half-duplex PCS helper;
  the IDA type at `0x19184` is now the recovered byte-selector/unsigned-result
  signature.
- `xpcs_set_vr_mii_an_ctrl_sgmii_link_sts @ 0x1964c`: complete and recorded in
  `functions/0x1964c-xpcs_set_vr_mii_an_ctrl_sgmii_link_sts.md`; source-like C
  is in `recovered/plat_smac.c`.
- It selects a companion PCS word at offset `0x7e0004` and replaces bit four
  with bit zero of the link-status input. Five direct callers discard its
  incidental pointer return. The IDA type at `0x1964c` is now the recovered
  void signature.
- `xpcs_set_vr_mii_an_ctrl_tx_config @ 0x195cc`: complete and recorded in
  `functions/0x195cc-xpcs_set_vr_mii_an_ctrl_tx_config.md`; source-like C is in
  `recovered/plat_smac.c`.
- It selects PCS word offset `0x7e0004` and replaces bit three with bit zero of
  a byte enable input. Its sole direct caller is `xpcs_init`; the IDA type is
  the recovered void two-byte signature.
- `xpcs_auto_negotiation_conf_in_sgmii_mode @ 0x196cc`: complete and recorded
  in `functions/0x196cc-xpcs_auto_negotiation_conf_in_sgmii_mode.md`;
  source-like C is in `recovered/plat_smac.c`.
- It rejects selectors above four and non-SGMII `sg_xpcs_mode` entries with
  `-1`. For mode three it disables AN, configures AN interrupt and MAC-auto
  switch using the low-byte flag, then only flag value one re-enables AN and
  writes `g_xmac_work_in_auto[xmac]`; every other flag value sets link status
  one without clearing that global. The corrected IDA signature now has both
  byte arguments and an `int` return.
- `xpcs_set_vr_mii_an_ctrl_mii_ctrl @ 0x19778`: complete and recorded in
  `functions/0x19778-xpcs_set_vr_mii_an_ctrl_mii_ctrl.md`; source-like C is in
  `recovered/plat_smac.c`.
- It selects PCS word offset `0x7e0004` and replaces bit eight with bit zero of
  a byte enable input. Its sole direct caller is `xpcs_init`; the IDA type is
  the recovered void two-byte signature.
- `xpcs_get_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta @ 0x197f8`: complete and
  recorded in `functions/0x197f8-xpcs_get_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta.md`;
  source-like C is in `recovered/plat_smac.c`.
- It reads bit zero of PCS offset `0x7e0008`, stores the normalized bit through
  an unchecked output pointer, and returns it. No direct code xrefs exist; IDA
  now has the recovered `(u8, u32 *) -> u32` signature.
- `xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw @ 0x192c8`: complete and recorded in
  `functions/0x192c8-xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw.md`; source-like C
  is in `recovered/plat_smac.c`.
- It selects PCS word offset `0x7e0000` and replaces bit nine with bit zero of
  a byte enable input. Four direct callers discard its incidental pointer
  return. The IDA type at `0x192c8` is now the recovered void byte signature.
- `xpcs_set_vr_mii_an_ctrl_an_intr_en @ 0x193c8`: complete and recorded in
  `functions/0x193c8-xpcs_set_vr_mii_an_ctrl_an_intr_en.md`; source-like C is
  in `recovered/plat_smac.c`.
- It selects PCS word offset `0x7e0004` and replaces bit zero with bit zero of
  a byte enable input. Five direct callers discard its incidental pointer
  return. The IDA type at `0x193c8` is now the recovered void byte signature.
- `xpcs_auto_negotiation_conf_in_usxgmii_mode @ 0x19448`: complete and recorded
  in `functions/0x19448-xpcs_auto_negotiation_conf_in_usxgmii_mode.md`;
  source-like C is in `recovered/plat_smac.c`.
- It rejects selectors above four with `-1`; otherwise it writes the low-byte
  flag through the VR-MII AN-interrupt and SR-MII AN writers. Exactly flag one
  also writes `g_xmac_work_in_auto[xmac]`; other values do not change that byte.
  The corrected IDA signature now has two byte inputs and an `int` return.
- `xpcs_1g_mode_conf @ 0x1952c`: complete and recorded in
  `functions/0x1952c-xpcs_1g_mode_conf.md`; source-like C is in
  `recovered/plat_smac.c`.
- Its verified ABI is `(xmac, speed, duplex, pcs_mode)`. It programs fixed 1G
  PCS settings, applies speed/duplex, enables low-power, waits for PSEQ state,
  and only on a zero wait result disables low-power and writes PCS mode. A
  nonzero wait returns `-1` while retaining low-power. The corrected IDA type
  now records these four arguments and `int` return.
- `xpcs_set_vr_mii_an_ctrl_pcs_mode @ 0x194b0`: complete and recorded in
  `functions/0x194b0-xpcs_set_vr_mii_an_ctrl_pcs_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It selects PCS word offset `0x7e0004` and replaces bits one/two with the low
  two bits of a raw PCS-mode input. Its two direct callers discard its
  incidental pointer return. The IDA type at `0x194b0` is now the recovered
  void `(u8, u32)` signature.
- `xpcs_prepare_for_switch_mode @ 0x19d5c`: complete and recorded in
  `functions/0x19d5c-xpcs_prepare_for_switch_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It always clears `g_xmac_work_in_auto[xmac]`, compares cached
  `sg_xpcs_mode[xmac]` with the target mode, and conditionally exits only mode
  three SGMII, modes five through seven USXGMII, or mode eight HSGMII. It
  discards every exit helper return and does not update the cached PCS mode.
  The IDA type at `0x19d5c` is now the recovered void signature.
- `xpcs_exit_sgmii_mode @ 0x19cd4`: complete and recorded in
  `functions/0x19cd4-xpcs_exit_sgmii_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears SR-MII AN enable, VR-MII AN interrupt enable, MAC-auto-switch,
  XS/PCS and MII 2.5G-mode flags, and SGMII link status in a fixed order. Its
  sole direct caller is `xpcs_prepare_for_switch_mode`; the IDA type at
  `0x19cd4` is now the recovered void byte signature.
- `xpcs_set_vr_mii_dig_ctrl1_2_5g_mode_en @ 0x191c8`: complete and recorded in
  `functions/0x191c8-xpcs_set_vr_mii_dig_ctrl1_2_5g_mode_en.md`; source-like C
  is in `recovered/plat_smac.c`.
- It selects PCS word offset `0x7e0000` and replaces bit two with bit zero of a
  byte enable input. Four direct callers discard its incidental pointer return.
  The IDA type at `0x191c8` is now the recovered void byte signature.
- `xpcs_set_vr_xs_pcs_dig_ctrl1_2_5g_mode_en @ 0x18dc4`: complete and recorded
  in `functions/0x18dc4-xpcs_set_vr_xs_pcs_dig_ctrl1_2_5g_mode_en.md`;
  source-like C is in `recovered/plat_smac.c`.
- It uses the parallel PCS word offset `0x0e0000` and replaces bit two with bit
  zero of a byte enable input. Four direct callers discard its incidental
  pointer return. The IDA type at `0x18dc4` is now the recovered void byte
  signature.
- `xpcs_exit_usxgmii_mode @ 0x19d30`: complete and recorded in
  `functions/0x19d30-xpcs_exit_usxgmii_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears the PCS USXGMII-enable control, then sets the VSMMD1-enable control
  in a fixed two-call sequence. Its sole direct caller is the USXGMII branch of
  `xpcs_prepare_for_switch_mode`; the IDA type at `0x19d30` is now the recovered
  void byte signature.
- `xpcs_exit_hsgmii_mode @ 0x19b60`: complete and recorded in
  `functions/0x19b60-xpcs_exit_hsgmii_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears MII 2.5G mode, MII VSMMD1, XS/PCS 2.5G mode, and XS/PCS VSMMD1 in
  that exact order. Its sole direct caller is the mode-eight branch of
  `xpcs_prepare_for_switch_mode`; the IDA type at `0x19b60` is now the recovered
  void byte signature.
- `xpcs_set_vr_mii_dig_ctrl1_vsmmd1_en @ 0x19348`: complete and recorded in
  `functions/0x19348-xpcs_set_vr_mii_dig_ctrl1_vsmmd1_en.md`; source-like C is
  in `recovered/plat_smac.c`.
- It selects PCS word offset `0x7e0000` and replaces bit 13 with bit zero of a
  byte enable input. Its two direct callers discard its incidental pointer
  return. The IDA type at `0x19348` is now the recovered void byte signature.
- `xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en @ 0x18ec4`: complete and recorded in
  `functions/0x18ec4-xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en.md`; source-like C
  is in `recovered/plat_smac.c`.
- It uses the parallel PCS word offset `0x0e0000` and replaces bit 13 with bit
  zero of a byte enable input. Four direct callers discard its incidental
  pointer return. The IDA type at `0x18ec4` is now the recovered void byte
  signature.
- `xpcs_set_vr_xs_pcs_dig_ctrl1_usxg_en @ 0x18930`: complete and recorded in
  `functions/0x18930-xpcs_set_vr_xs_pcs_dig_ctrl1_usxg_en.md`; source-like C
  is in `recovered/plat_smac.c`.
- It selects PCS word offset `0x0e0000` and replaces bit nine with bit zero of
  a byte enable input. Its two direct callers discard its incidental pointer
  return. The IDA type at `0x18930` is now the recovered void byte signature.
- `xpcs_usxgmii_mode_conf @ 0x1a210`: complete and recorded in
  `functions/0x1a210-xpcs_usxgmii_mode_conf.md`; source-like C is in
  `recovered/plat_smac.c`.
- It validates byte selectors through four, prepares a transition using fixed
  target mode five, enables USXG/VSMMD1, replaces bits 10 through 12 of the
  `0x0e001c` PCS word with `usxg_mode & 7`, requests VR reset, and ignores the
  reset-wait result. Only afterward does it accept mode codes zero through five
  and map `{0,3}` to cached mode five, `{1,4}` to six, and `{2,5}` to seven;
  unsupported mode codes return `-1` after the hardware writes. The IDA type at
  `0x1a210` is now the recovered `(u8, u32)` `int` signature.
- `xpcs_set_vr_xs_pcs_dig_ctrl1_vr_rst @ 0x189b0`: complete and recorded in
  `functions/0x189b0-xpcs_set_vr_xs_pcs_dig_ctrl1_vr_rst.md`; source-like C is
  in `recovered/plat_smac.c`.
- It selects PCS word offset `0x0e0000` and replaces bit 15 with bit zero of a
  byte enable input. Its sole direct caller discards its incidental pointer
  return. The IDA type at `0x189b0` is now the recovered void byte signature.
- `xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared @ 0x18b3c`: complete and
  recorded in `functions/0x18b3c-xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared.md`;
  source-like C is in `recovered/plat_smac.c`.
- It polls PCS VR reset bit 15 at offset `0x0e0000` for at most 400 iterations,
  delaying `859000` through `__const_udelay` after every set-bit observation.
  It returns zero once clear and `-1` after the final delay. Two speed/duplex
  callers propagate the result; `xpcs_usxgmii_mode_conf` ignores it. The IDA
  type at `0x18b3c` is now the recovered byte-selector `int` signature.
- `xpcs_set_vr_xs_pcs_dig_ctrl1_usra_rst_en @ 0x18e44`: complete and recorded
  in `functions/0x18e44-xpcs_set_vr_xs_pcs_dig_ctrl1_usra_rst_en.md`;
  source-like C is in `recovered/plat_smac.c`.
- It selects PCS word offset `0x0e0000` and replaces bit 10 with bit zero of a
  byte enable input. Its two direct callers discard its incidental pointer
  return. The IDA type at `0x18e44` is now the recovered void byte signature.
- `xpcs_speed_duplex_conf_in_auto_disable_usxgmii_mode @ 0x19090`: complete and
  recorded in
  `functions/0x19090-xpcs_speed_duplex_conf_in_auto_disable_usxgmii_mode.md`;
  source-like C is in `recovered/plat_smac.c`.
- It validates selectors through four, writes speed and duplex, delays 859000,
  enables USRA reset, and returns the VR-reset wait status. The saved third ABI
  argument is duplex; the IDA type at `0x19090` is now the recovered
  `(u8, u32, u32)` `int` signature. No direct code xrefs are currently present
  in the IDB.
- `xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode @ 0x19910`: complete and
  recorded in
  `functions/0x19910-xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode.md`;
  source-like C is in `recovered/plat_smac.c`.
- It reads an AN status snapshot at offset `0x7e0008`; missing bit zero returns
  `-1`, while bit-zero-without-bit-14 clears auto status and returns zero. When
  bit 14 is set it clears completion status, maps bits 10 through 12 to UNI
  speed, uses bit 13 as duplex, writes the PCS controls, delays, enables USRA
  reset, commits all output pointers, then returns the reset-wait status. The
  IDA type at `0x19910` is the recovered four-argument `int` signature.
- `xpcs_clear_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta @ 0x19840`: complete and
  recorded in
  `functions/0x19840-xpcs_clear_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta.md`;
  source-like C is in `recovered/plat_smac.c`.
- It selects AN status word offset `0x7e0008` and clears only bit zero through a
  volatile RMW. Its two direct callers discard its incidental pointer return.
  The IDA type at `0x19840` is now the recovered void byte signature.
- `xpcs_switch_vr_mii_an_intr_sts_speed @ 0x198b4`: complete and recorded in
  `functions/0x198b4-xpcs_switch_vr_mii_an_intr_sts_speed.md`; source-like C is
  in `recovered/plat_smac.c`.
- It maps raw AN speed codes `{0,1,2,3,4,5}` to UNI codes `{1,2,3,6,4,5}`;
  all other unsigned values become seven. It writes and returns the mapped
  value. The IDA type at `0x198b4` is now the recovered `(u32, u32 *)` unsigned
  signature.
- `xpcs_get_speed_duplex_in_auto_en_sgmii_mode @ 0x19bdc`: complete and
  recorded in
  `functions/0x19bdc-xpcs_get_speed_duplex_in_auto_en_sgmii_mode.md`;
  source-like C is in `recovered/plat_smac.c`.
- It reads AN status snapshot offset `0x7e0008`; missing bit zero returns `-1`.
  With bit zero set it clears completion state, writes auto status zero when
  bit four is clear, or maps bits two/three to UNI speed and uses bit one as
  duplex when bit four is set. It returns zero for either bit-zero completion
  branch without reprogramming PCS. The IDA type at `0x19bdc` is the recovered
  four-argument `int` signature.
- `xpcs_wait_speed_duplex_conf_in_auto_en_usxgmii_mode @ 0x1a370`: complete and
  recorded in
  `functions/0x1a370-xpcs_wait_speed_duplex_conf_in_auto_en_usxgmii_mode.md`;
  source-like C is in `recovered/plat_smac.c`.
- It initializes local speed, duplex, and auto-status words, calls the USXGMII
  auto-enable handler up to 400 times, and returns immediately on zero. Each
  nonzero result delays 859000; after the final delay it logs and returns the
  final status. The locals are never externally exposed. The IDA type at
  `0x1a370` is now the recovered byte-selector `int` signature; no direct code
  xrefs exist in the current IDB.
- `xpcs_set_sr_xs_pcs_ctrl1_low_power_en @ 0x18870`: complete and recorded in
  `functions/0x18870-xpcs_set_sr_xs_pcs_ctrl1_low_power_en.md`; source-like C
  is in `recovered/plat_smac.c`.
- It selects PCS word offset `0x0c0000` and replaces bit 11 with bit zero of a
  byte enable input. Eight direct callers discard its incidental pointer return.
  The IDA type at `0x18870` is now the recovered void byte signature.
- `xpcs_set_sr_xs_pcs_ctrl2_pcs_type @ 0x188f0`: complete and recorded in
  `functions/0x188f0-xpcs_set_sr_xs_pcs_ctrl2_pcs_type.md`; source-like C is in
  `recovered/plat_smac.c`.
- It directly replaces the selector-specific PCS word at offset `0x0c001c` with
  a raw type word; no masking or readback occurs. Six configuration callers
  discard its incidental pointer return. The IDA type at `0x188f0` is now the
  recovered void `(u8, u32)` signature.
- `xpcs_set_vr_xs_pcs_xaui_ctrl_xaui_mode.constprop.1 @ 0x18c68`: complete and
  recorded in
  `functions/0x18c68-xpcs_set_vr_xs_pcs_xaui_ctrl_xaui_mode-constprop-1.md`;
  source-like C is in `recovered/plat_smac.c`.
- It selects PCS word offset `0x0e0010` and clears bit zero through a volatile
  RMW. Its two direct callers discard its incidental pointer return. The IDA
  type at `0x18c68` is now the recovered void byte signature.
- `xpcs_set_sr_xs_pcs_ctrl1_speed_sel.constprop.2 @ 0x18cdc` and
  `xpcs_set_sr_pma_ctrl_speed_sel.constprop.3 @ 0x18d50`: complete and recorded
  in `functions/`; source-like C is in `recovered/plat_smac.c`.
- Both clear bit 13 through a volatile RMW: the former operates on offset
  `0x0c0000` and has two callers; the latter operates on `0x040000` and has
  three. Their IDA types are now recovered void byte signatures.
- `xpcs_eee_cfg @ 0x18a30`: complete and recorded in
  `functions/0x18a30-xpcs_eee_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Valid selectors through four receive fixed writes `0x81df` at `0x0e0018` and
  `0x1cf2` at `0x0e0020`; offset `0x0e0024` receives `0x2ffa` only when the
  byte profile equals one, otherwise `0x35fa`. Invalid selectors log and return.
  No direct code xrefs exist; the IDA type is now recovered void `(u8, u8)`.
- `0x18de4` is a tail entry within completed
  `xpcs_set_vr_xs_pcs_dig_ctrl1_2_5g_mode_en @ 0x18dc4`, not an independent
  function. No duplicate recovery was added.
- Next function: `xpcs_2p5gbase_x_conf @ 0x19ee0`.
- `xpcs_2p5gbase_x_conf @ 0x19ee0`: complete; it configures type `0x0e`, pulses
  PMA low power around delay `859000`, then caches mode four. Next:
- `xpcs_set_sr_pma_ctrl1_low_power_en @ 0x187f0`: complete; it RMWs bit 11 of
  the selector-specific PMA control word at `0x040000`. Its two callers are the
  2.5GBASE-X low-power enable/disable writes.
- `xpcs_10gbase_r_conf @ 0x19dcc`: complete; it prepares mode zero, writes type
  zero, pulses SR-XS/PCS low power around delay `859000`, then caches mode zero.
- `xpcs_5gbase_r_conf @ 0x19e54`: complete; it applies the same low-power pulse
  with target/cache mode one and type `0x0f`. Next: `xpcs_hsgmii_mode_conf @ 0x19fe8`.
- `xpcs_hsgmii_mode_conf @ 0x19fe8`: complete. It configures all HSGMII PCS
  controls, returns `-1` if PSEQ wait fails, otherwise configures AN/timers and
  caches mode eight. CPU 129/133 alone apply the auto-enable branch. Next:
  `xpcs_set_vr_mii_link_timer_ctrl @ 0x19a50`.
- `xpcs_set_vr_mii_link_timer_ctrl @ 0x19a50` and
  `xpcs_set_sr_mii_dig_ctrl1_cl37_tmr_ovr_ride @ 0x19248`: complete. The former
  directly stores a raw timer at `0x7e0028`; the latter replaces bit three at
  `0x7e0000`. Next: `xpcs_auto_negotiation_conf_in_1000base_x_mode @ 0x19a90`.
- `xpcs_auto_negotiation_conf_in_1000base_x_mode @ 0x19a90`: complete. Its
  three byte inputs are XMAC selector, auto-enable, and 2.5G-enable; it only
  configures cached mode three. Next: `xpcs_1000base_x_conf @ 0x19f74`.
- `xpcs_1000base_x_conf @ 0x19f74`: complete; it prepares mode two, calls the
  1G PCS sequence with PCS mode zero, then caches mode two even on failure.
- `xpcs_sgmii_mode_conf @ 0x1a19c`: complete; it mirrors the 1000BASE-X wrapper
  but prepares/caches mode three and passes PCS mode two. Next: PSEQ wait helper
  `xpcs_wait_vr_xs_pcs_dig_sts_pseq_state.constprop.0 @ 0x18bcc`.
- `xpcs_wait_vr_xs_pcs_dig_sts_pseq_state.constprop.0 @ 0x18bcc`: complete.
  It polls bits 2-4 of `0x0e0040` until not four, up to 400 delayed iterations.
  `0x18f4c` is an internal tail entry of the completed SR-MII speed writer, not
  an independent function.
- `xpcs_init @ 0x1a420`: complete and recorded in `functions/0x1a420-xpcs_init.md`;
  source-like C is in `recovered/plat_smac.c`.
- It accepts selectors zero through four, waits for reset bit 15 to clear in
  `0x0c0000` and `0x7c0000` with independent 400-iteration delay loops, then
  clears TX-config and MII-control. Its only direct caller ignores its `int`
  status; IDA now has the recovered `int (u8)` signature.
- `byPassEnableSet @ 0x1a550`: complete and recorded in
  `functions/0x1a550-byPassEnableSet.md`; source-like C is in
  `recovered/plat_smac.c`.
- On CPU 133 or 129 only, it RMWs bit four of PCS offset `0x0e0014`, setting it
  for any nonzero byte enable and clearing it for zero. Nine XMAC mode paths
  use it and discard its always-zero `int` status.
- `phy_zx5201_check @ 0x1a608`: complete and recorded in
  `functions/0x1a608-phy_zx5201_check.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reads extended MDIO register 26 twice, gates on bit six, maps bits 8-9 to
  a low status code, and packs bit seven into return bit 10. There are no direct
  code xrefs; IDA now has the recovered `int (u8)` signature.
- `phy_zx5201_init @ 0x1a688`: complete and recorded in
  `functions/0x1a688-phy_zx5201_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- It runs a fixed extended-MDIO script on the selected PHY and byte-truncated
  adjacent PHY, with 11 writes, two reads, nine `429500` delays, and one masked
  register-21 RMW. It returns the final `printk` result and has no direct xrefs.
- `phy_8574_check @ 0x1a818`: complete and recorded in
  `functions/0x1a818-phy_8574_check.md`; source-like C is in
  `recovered/plat_smac.c`.
- It gates on basic register-one bit two, saves/restores page register 31 around
  two reads of temporary-page register 28, then maps bits 3-5 into a packed
  integer result. There are no direct code xrefs; IDA has `int (u8)`.
- `phy_8574_init @ 0x1a8f0`: complete and recorded in
  `functions/0x1a8f0-phy_8574_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- Its unprotected module-wide byte runs a first-call register-18 poll once even
  after timeout, then each call executes a fixed page/register script with three
  masked writes and 13 `429500` delays. No direct xrefs; IDA has `int (u8)`.
- `zte_gephy_set_eee_en @ 0x1abb0`: complete and recorded in
  `functions/0x1abb0-zte_gephy_set_eee_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- It writes MDIO register 16 then RMWs register 17 bits one/two, setting them
  only when its byte enable equals one. No direct xrefs; IDA has `int (u8, u8)`.
- `zte_gephy_set_energy_detect_power_down_en @ 0x1ac10`: complete and recorded
  in `functions/0x1ac10-zte_gephy_set_energy_detect_power_down_en.md`;
  source-like C is in `recovered/plat_smac.c`.
- It clears MDIO register-21 bit three, then ORs the unmasked byte enable shifted
  by three. No direct xrefs; IDA has `int (u8, u8)`.
- `zte_gephy_set_link_status_change_en @ 0x1ac58`: complete and recorded in
  `functions/0x1ac58-zte_gephy_set_link_status_change_en.md`; source-like C is
  in `recovered/plat_smac.c`.
- It replaces MDIO register-24 bit two from the low bit of a halfword enable.
  No direct xrefs; IDA has `int (u8, u16)`.
- `zte_gephy_get_eee_en_status @ 0x1aca4`: complete and recorded in
  `functions/0x1aca4-zte_gephy_get_eee_en_status.md`; source-like C is in
  `recovered/plat_smac.c`.
- It rejects null output pointers, otherwise writes MDIO register-17 bits one/two
  to an output byte after a register-16 selection write. No direct xrefs; IDA
  has `int (u8, u8 *)`.
- `zte_gephy_get_short_reach_en @ 0x1ad04`: complete and recorded in
  `functions/0x1ad04-zte_gephy_get_short_reach_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- It rejects null output, returns zero without an output store for an empty
  APB-base slot, and otherwise returns APB offset `0x90` bit zero in a byte.
  No direct xrefs; IDA has `int (u8, u8 *)`.
- `zte_gephy_get_energy_detect_power_down_en @ 0x1ad58`: complete and recorded
  in `functions/0x1ad58-zte_gephy_get_energy_detect_power_down_en.md`;
  source-like C is in `recovered/plat_smac.c`.
- It rejects null output, otherwise discards an MDIO register-21 read and stores
  zero to the output byte. No direct xrefs; IDA has `int (u8, u8 *)`.
- `zte_gephy_get_1000m_tx_dac_lv @ 0x1ada0`: complete and recorded in
  `functions/0x1ada0-zte_gephy_get_1000m_tx_dac_lv.md`; source-like C is in
  `recovered/plat_smac.c`.
- It rejects null output, selects MDIO register 17 through register 16 value
  `0xffffb407`, then stores the low six bits through a halfword pointer.
- `zte_gephy_get_1000m_tx_dac_slew @ 0x1ae00`: complete and recorded in
  `functions/0x1ae00-zte_gephy_get_1000m_tx_dac_slew.md`; source-like C is in
  `recovered/plat_smac.c`.
- It rejects null output, selects MDIO register 17 through register 16 value
  `0xffffb409`, then stores the low three bits through a halfword pointer.
- `zte_gephy_get_100m_tx_dac_lv @ 0x1ae60`: complete and recorded in
  `functions/0x1ae60-zte_gephy_get_100m_tx_dac_lv.md`; source-like C is in
  `recovered/plat_smac.c`.
- It rejects null output, selects MDIO register 17 through register 16 value
  `0xffffb406`, then stores the low six bits through a halfword pointer.
- `zte_gephy_get_100m_tx_dac_slew @ 0x1aec0`: complete and recorded in
  `functions/0x1aec0-zte_gephy_get_100m_tx_dac_slew.md`; source-like C is in
  `recovered/plat_smac.c`.
- It rejects null output, selects MDIO register 17 through register 16 value
  `0xffffb408`, then stores the low three bits through a halfword pointer.
- `zte_gephy_get_link_status_change_en @ 0x1af20`: complete and recorded in
  `functions/0x1af20-zte_gephy_get_link_status_change_en.md`; source-like C is
  in `recovered/plat_smac.c`.
- It rejects null output, otherwise reads MDIO register-24 bit two into a
  halfword. No direct xrefs; IDA has `int (u8, u16 *)`.
- `zte_gephy_get_link_status_change_event @ 0x1af6c`: complete and recorded in
  `functions/0x1af6c-zte_gephy_get_link_status_change_event.md`; source-like C
  is in `recovered/plat_smac.c`.
- It rejects null output, otherwise reads MDIO register-25 bit two into a
  halfword. No direct xrefs; IDA has `int (u8, u16 *)`.
- `zte_gephy_get_rx_stats @ 0x1afb8`: complete and recorded in
  `functions/0x1afb8-zte_gephy_get_rx_stats.md`; source-like C is in
  `recovered/plat_smac.c`.
- It logs MDIO register-20 CRC count and two register-17 counter halves selected
  by values `0xffff9409`/`0xffff940a`; it returns the final `printk` result.
- `zte_gephy_set_short_reach_en @ 0x1b048`: complete and recorded in
  `functions/0x1b048-zte_gephy_set_short_reach_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- It calls `apb_bit_write(base + 0x90, enable, 1, 0)` only for a nonzero
  byte-indexed APB-base slot and otherwise returns zero.
- `zte_gephy_set_ref_clk_25M @ 0x1b08c`: complete and recorded in
  `functions/0x1b08c-zte_gephy_set_ref_clk_25M.md`; source-like C is in
  `recovered/plat_smac.c`.
- It returns the APB address pointer from
  `apb_bit_write(gephy_apb_base + 0x200018, enable & 1, 1, 12)`; no direct code
  xrefs exist.
- `zte_gephy_set_100m_tx_dac_lv @ 0x1b0bc`: complete and recorded in
  `functions/0x1b0bc-zte_gephy_set_100m_tx_dac_lv.md`; source-like C is in
  `recovered/plat_smac.c`.
- It rejects halfword levels above 63, then replaces MDIO register-17 low six
  bits after selection value `0xffffb406`. No direct xrefs exist.
- `zte_gephy_set_1000m_tx_dac_slew @ 0x1b130`: complete and recorded in
  `functions/0x1b130-zte_gephy_set_1000m_tx_dac_slew.md`; source-like C is in
  `recovered/plat_smac.c`.
- It rejects halfword slew values above seven, then replaces MDIO register-17
  low three bits after selection value `0xffffb409`. No direct xrefs exist.
- `zte_gephy_set_100m_tx_dac_slew @ 0x1b1a4`: complete and recorded in
  `functions/0x1b1a4-zte_gephy_set_100m_tx_dac_slew.md`; source-like C is in
  `recovered/plat_smac.c`.
- It rejects halfword slew values above seven, then replaces MDIO register-17
  low three bits after selection value `0xffffb408`. No direct xrefs exist.
- `zte_gephy_set_1000m_tx_dac_lv @ 0x1b218`: complete and recorded in
  `functions/0x1b218-zte_gephy_set_1000m_tx_dac_lv.md`; source-like C is in
  `recovered/plat_smac.c`.
- It rejects halfword levels above 63, then replaces MDIO register-17 low six
  bits after selection value `0xffffb407`. No direct xrefs exist.
- `check_phy_gephy @ 0x1b28c`: complete and recorded in
  `functions/0x1b28c-check_phy_gephy.md`; source-like C is in
  `recovered/plat_smac.c`.
- It saves/restores MDIO register 30 around a discarded/delayed double read of
  register 26, then maps bits 6-9 to the packed GEPHY probe result. SMAC init
  references it as a callback-table entry; IDA has `int (u8)`.
- `phy_zxicge_init @ 0x1b340`: complete and recorded in
  `functions/0x1b340-phy_zxicge_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- Its byte callback parameter is unused. Its unprotected byte guard derives four
  contiguous APB base slots from `gephy_apb_base` once, logs, returns one, and
  later returns the stored nonzero guard. SMAC init references it through a
  callback table.
- `zte_set_gephy_enable @ 0x1b3a0`: complete and recorded in
  `functions/0x1b3a0-zte_set_gephy_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- Only enable byte value one clears MDIO register-zero bit 11; every other value
  sets it. SMAC init references it through a callback table.
- `zte_get_gephy_enable @ 0x1b3f4`: complete and recorded in
  `functions/0x1b3f4-zte_get_gephy_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It writes active-low MDIO register-zero bit 11 through an unchecked byte
  pointer and returns zero. SMAC init references it through a callback table.
- `phy_zxic051_get_linkstate @ 0x1b430`: complete and recorded in
  `functions/0x1b430-phy_zxic051_get_linkstate.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reads low bytes at contiguous state-array indices `phy`, `phy + 4`, and
  `phy + 8` into link, speed, and duplex outputs, then converts the speed in
  place. XMAC ZXIC PHY setup references it through a callback table.
- `phy_zxic051_set_enable @ 0x1b474`: complete and recorded in
  `functions/0x1b474-phy_zxic051_set_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It writes the byte enable as a full state-array word at `phy + 12`, then calls
  a no-argument external notifier. XMAC ZXIC PHY setup references it through a
  callback table.
- `phy_zxic051_get_enable @ 0x1b4a4`: complete and recorded in
  `functions/0x1b4a4-phy_zxic051_get_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It has no arguments and returns the direct external getter result. XMAC ZXIC
  PHY setup references it through a callback table.
- `phy_zxic051_set_linkmode @ 0x1b4b8`: complete and recorded in
  `functions/0x1b4b8-phy_zxic051_set_linkmode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It uses a wrapped `(u8)(phy + 4)` PHY lookup and signed callback-table slot.
  Valid force/speed/duplex combinations perform four indirect MDIO writes; ID
  `0xff` returns `-1` after a rate-limited optional log. XMAC ZXIC PHY setup
  references it through a callback table.
- `phy_zxic051_set_loopback @ 0x1b794`: complete and recorded in
  `functions/0x1b794-phy_zxic051_set_loopback.md`; source-like C is in
  `recovered/plat_smac.c`.
- It performs PHY-ID-gated indirect MDIO RMW scripts for exact enable value one
  and all other values, including fixed masks and `4295000` delays. XMAC ZXIC
  PHY setup references it through a callback table.
- `phy_zxic051_get_loopback @ 0x1b9e0`: complete and recorded in
  `functions/0x1b9e0-phy_zxic051_get_loopback.md`; source-like C is in
  `recovered/plat_smac.c`.
- It uses the same wrapped PHY lookup and signed callback-table slot to read GE
  register-zero bit 14 into an unchecked byte. PHY ID `0xff` returns `-1`.
- `phy_zxic051_get_linkmode @ 0x1ba78`: complete and recorded in
  `functions/0x1ba78-phy_zxic051_get_linkmode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It has no arguments and returns the direct external linkmode getter result.
  XMAC ZXIC PHY setup references it through a callback table.
- `phy_zxic051_init_check @ 0x1ba8c`: complete and recorded in
  `functions/0x1ba8c-phy_zxic051_init_check.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reads register-31 selectors 41 and 28 through signed `(phy - 4)` reader
  slot; only values 0/1 invoke external PHY init and return `-1`. Its caller is
  `phy_zxic051_para_init`.
- `phy_zxic051_para_init @ 0x1bb1c`: complete and recorded in
  `functions/0x1bb1c-phy_zxic051_para_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- It discards a first PHY-ID lookup then returns `phy_zxic051_init_check(phy)`.
  Its sole caller is `xmac_zxic_phy_init`.
- `phy_zxic051_port_exist @ 0x1bb48`: complete and recorded in
  `functions/0x1bb48-phy_zxic051_port_exist.md`; source-like C is in
  `recovered/plat_smac.c`.
- It calls external port-use state for all inputs but returns one only for port
  five when that state is nonzero. Its sole caller is `xmac_zxic_phy_init`.
- `phy_zxic_051_phy_uni_check @ 0x1bb78`: complete and recorded in
  `functions/0x1bb78-phy_zxic_051_phy_uni_check.md`; source-like C is in
  `recovered/plat_smac.c`.
- It is the external PHY link state machine: it initializes caller outputs,
  rejects ports zero through three, gates NBASEx ports, and drives distinct
  link-up/down MDIO/APB/counter/cache transitions. Its sole caller is
  `phy_zxic051_check`; IDA has the recovered five-argument `int` signature.
- `plat_cleanupModule @ 0x1c3e8`: complete and recorded in
  `functions/0x1c3e8-plat_cleanupModule.md`; source-like C is in
  `recovered/plat_smac.c`.
- Module cleanup is void and calls `nppt_exit()` then
  `pon_driver_unregister()`; neither consumes a status argument.
- `dg_timer_init @ 0x1c400`: complete and recorded in
  `functions/0x1c400-dg_timer_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reinitializes `dg_timer`, writes its `+0x10` expires word as `jiffies +
  500`, and returns `add_timer` status. Both direct callers are in `zx_pon_int`.
- `__fswab64 @ 0x1c458`: complete and recorded in
  `functions/0x1c458-fswab64.md`; source-like C is in `recovered/plat_smac.c`.
- It is a pure unsigned 64-bit byte reversal (`REV X0,X0`), called twice by
  `dump_net_condition_set`.
- `_idm_rx_refill @ 0x1c460`: complete and recorded in
  `functions/0x1c460-idm_rx_refill.md`; source-like C is in
  `recovered/plat_smac.c`.
- It allocates an IDM buffer, converts `buffer + uBP_BUFFER_OFFSET + 64` to a
  physical address, byte-swaps it to a descriptor word, and returns `-1` on
  allocation failure. Two `idm_init` call sites use it.
- `getEponDeactiveState @ 0x0`: complete and recorded in
  `functions/0x00000-getEponDeactiveState.md`; source-like C is in
  `recovered/plat_smac.c`.
- It returns the unsynchronized full 32-bit `g_epon_deactive` global; no direct
  code xrefs exist.
- `setEponDeactiveState @ 0xc`: complete and recorded in
  `functions/0x0000c-setEponDeactiveState.md`; source-like C is in
  `recovered/plat_smac.c`.
- It normalizes an integer to zero or one in `g_epon_deactive` and returns the
  incidental address held in X0. No direct code xrefs exist.
- `pon_set_8k_out_en @ 0x20`: complete and recorded in
  `functions/0x00020-pon_set_8k_out_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- It conditionally RMWs pin-mux bits 1-2 with equality-one input semantics. Its
  sole direct caller is the PON PLL reference selector.
- `pon_set_1pps_out_en @ 0x60`: complete and recorded in
  `functions/0x00060-pon_set_1pps_out_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- It conditionally RMWs `pin_mux_base + 8` bits 21-23 with equality-one input
  semantics. Its sole direct caller is PON ToD output control.
- `pon_set_uart1_txd_en @ 0xa0`: complete and recorded in
  `functions/0x000a0-pon_set_uart1_txd_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- It conditionally RMWs base pin-mux bits 27-28 with equality-one input
  semantics. Its sole direct caller is PON ToD output control.
- `pon_set_1pps_tod_out_en @ 0xdc`: complete and recorded in
  `functions/0x000dc-pon_set_1pps_tod_out_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- It forwards one byte enable to ordered 1PPS and UART1 pin-mux writers, ignores
  their return values, and returns zero. No direct code xrefs exist.
- `pon_set_pin_mux_13 @ 0x104`: complete and recorded in
  `functions/0x00104-pon_set_pin_mux_13.md`; source-like C is in
  `recovered/plat_smac.c`.
- It replaces base pin-mux bits 25-26 from the low two bits of its byte input.
  Its sole direct caller is the PON PLL reference selector.
- `pon_set_pll_pon_ref_clock @ 0x128`: complete and recorded in
  `functions/0x00128-pon_set_pll_pon_ref_clock.md`; source-like C is in
  `recovered/plat_smac.c`.
- It replaces `top_crm_base + 0x10` bits 4-5 from byte input low bits. Its sole
  direct caller is the PON PLL reference selector.
- `pon_set_pll_pon_cfg_with_ref_clk_25M @ 0x14c`: complete and recorded in
  `functions/0x0014c-pon_set_pll_pon_cfg_with_ref_clk_25M.md`; source-like C
  is in `recovered/plat_smac.c`.
- It writes a fixed CRM PLL profile at offsets `0xc0`-`0xcc`, then sets bit 28
  at `0xc4`. Its sole direct caller is the PON PLL reference selector.
- `pon_set_pll_pon_en @ 0x188`: complete and recorded in
  `functions/0x00188-pon_set_pll_pon_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- It replaces CRM offset `0xc4` bit 28 from input bit zero. No direct code xrefs
  exist.
- `pon_use_pll_pon_ref_from_ex_pll @ 0x1ac`: complete and recorded in
  `functions/0x001ac-pon_use_pll_pon_ref_from_ex_pll.md`; source-like C is in
  `recovered/plat_smac.c`.
- It calls three literal-one PON pin/clock selectors then writes the fixed 25 MHz
  PLL profile, ignores all child results, and returns zero.
- `isCpuType_133 @ 0x1dc`: complete and recorded in
  `functions/0x001dc-isCpuType_133.md`; source-like C is in
  `recovered/plat_smac.c`.
- It returns one exactly for `g_pon_cputype == 2`; 67 PON, SerDes, IDM, and
  XMAC/PCS code paths call it directly.
- `isCpuType_132 @ 0x1f0`: complete and recorded in
  `functions/0x001f0-isCpuType_132.md`; source-like C is in
  `recovered/plat_smac.c`.
- It returns one exactly for `g_pon_cputype == 1`; 30 PON, SerDes, IDM, and
  XMAC code paths call it directly.
- `isCpuType_129 @ 0x204`: complete and recorded in
  `functions/0x00204-isCpuType_129.md`; source-like C is in
  `recovered/plat_smac.c`.
- It returns one exactly for `g_pon_cputype == 4`; 53 PON, SerDes, IDM, and
  XMAC/PCS code paths call it directly.
- `ponserdes_to_xmac1_en_set @ 0x218`: complete and recorded in
  `functions/0x00218-ponserdes_to_xmac1_en_set.md`; source-like C is in
  `recovered/plat_smac.c`.
- Inputs zero/one configure shared clock. Input one selects XMAC1 while all
  other values write inverse PON/NPPT hardware state; its sole caller is
  `zx_pon_probe`.
- `pon_sys_soft_reset @ 0x274`: complete and recorded in
  `functions/0x00274-pon_sys_soft_reset.md`; source-like C is in
  `recovered/plat_smac.c`.
- It pulses NPPT offset `0x2c0004` bit 31 low then high with `1718000` delay
  loops and diagnostic logs. Its sole caller is `zx_pon_probe`.
- `arm64_kernel_use_ng_mappings @ 0x324`: complete and recorded in
  `functions/0x00324-arm64_kernel_use_ng_mappings.md`; source-like C is in
  `recovered/plat_smac.c`.
- If constant capabilities are ready it tests `cpu_hwcap_keys[23] > 0`; otherwise
  it reads `cpu_hwcaps` bit 23. Three `zx_pon_probe` call sites use it.
- `pon_int_enable @ 0x35c`: complete and recorded in
  `functions/0x0035c-pon_int_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears the input mask at PON offset `0x44`, writes and returns the updated
  32-bit word. Eight interrupt registration helpers call it.
- `nppt_int_enable @ 0x374`: complete and recorded in
  `functions/0x00374-nppt_int_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears the input mask at NPPT offset `0x4`, writes and returns the updated
  32-bit word. PTP, PTP timestamp, and OAM registration helpers call it.
- `pon_soc_pon_core_clk_init @ 0x38c`: complete and recorded in
  `functions/0x0038c-pon_soc_pon_core_clk_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- It only applies CPU 133/129-specific CRM offset `0xc` clock fields and returns
  zero. Its sole caller is `zx_pon_probe`.
- `pon_soc_pon_cci_clk_init @ 0x3e0`: complete and recorded in
  `functions/0x003e0-pon_soc_pon_cci_clk_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- It sets CRM offset `0x4` bits 4-5 and returns zero. No direct code xrefs exist.
- `pon_soc_pon_woe0_clk_init @ 0x3fc`: complete and recorded in
  `functions/0x003fc-pon_soc_pon_woe0_clk_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- It sets CRM offset `0xc` bits covered by `0x00700000` and returns zero. No
  direct code xrefs exist.
- `pon_soc_pon_woe1_clk_init @ 0x418`: complete and recorded in
  `functions/0x00418-pon_soc_pon_woe1_clk_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- It sets CRM offset `0xc` bits covered by `0x07000000` and returns zero. No
  direct code xrefs exist.
- `pon_soc_pon_tm_clk_init @ 0x434`: complete and recorded in
  `functions/0x00434-pon_soc_pon_tm_clk_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- It sets CRM offset `0xc` bits zero and one, then returns zero. No direct code
  xrefs exist.
- `nppt_idm_cci_enable @ 0x450`: complete and recorded in
  `functions/0x00450-nppt_idm_cci_enable.md`; source-like C is in
  `recovered/plat_smac.c`.
- It writes fixed `0x00200020` IDM CCI values at system-control offsets `0x78`
  and `0x7c`, then returns logging status. Its sole caller is `zx_pon_probe`.
- `pon_soc_pon_cci_aclk_init @ 0x480`: complete and recorded in
  `functions/0x00480-pon_soc_pon_cci_aclk_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- It sets CRM offset `0x4` bits 4-6 and returns zero. Its sole caller is
  `zx_pon_probe`.
- `pon_soc_pon_tm_aclk_init @ 0x49c`: complete and recorded in
  `functions/0x0049c-pon_soc_pon_tm_aclk_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- CPU 129 sets CRM offset `0xc` low two bits; all other CPU types set low three
  bits. Its sole caller is `zx_pon_probe`.
- `pon_soc_pon_nppt_clk_init @ 0x4d4`: complete and recorded in
  `functions/0x004d4-pon_soc_pon_nppt_clk_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- It programs revision-specific CRM offset `0xc` mux fields, sets offset `0x48`
  bit 10, logs both resulting words, and returns zero. Its sole caller is
  `zx_pon_probe`.
- `pon_soc_pon_woe_clk_init @ 0x548`: complete and recorded in
  `functions/0x00548-pon_soc_pon_woe_clk_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- CPU 129 sets CRM offset `0xc` bits `0x000c0000`; other CPU types set
  `0x00700000`. Its sole caller is `zx_pon_probe`.
- `pon_soc_pon_rgmii_clk_set @ 0xdcc`: complete and recorded in
  `functions/0x00dcc-pon_soc_pon_rgmii_clk_set.md`; source-like C is in
  `recovered/plat_smac.c`.
- It replaces CRM offset `0xc` bit 16 with `enable == 0`. Its sole caller is
  `nppt_smac_set_rgmii_mode`.
- `pps_reset @ 0xdf4`: complete and recorded in
  `functions/0x00df4-pps_reset.md`; source-like C is in
  `recovered/plat_smac.c`.
- It masks PPS offset `0xc` with `0xfff85400`, delays, restores bits using
  `0x0007abff`, logs all phases, and returns zero. No direct code xrefs exist.
- `pon_driver_unregister @ 0xeb4`: complete and recorded in
  `functions/0x00eb4-pon_driver_unregister.md`; source-like C is in
  `recovered/plat_smac.c`.
- It is a no-argument void wrapper around the fixed `zx_pon_driver` platform
  unregister call. Its sole caller is `plat_cleanupModule`.
- `register_gmac_int @ 0xfa8`: complete and recorded in
  `functions/0x00fa8-register_gmac_int.md`; source-like C is in
  `recovered/plat_smac.c`.
- It stores a machine-word callback/context pair then returns
  `pon_int_enable(1)`. No direct code xrefs exist.
- `register_xgmac_int @ 0xfcc`: complete and recorded in
  `functions/0x00fcc-register_xgmac_int.md`; source-like C is in
  `recovered/plat_smac.c`.
- It stores the XGPON ISR callback and shared context then returns
  `pon_int_enable(0x80)`. No direct code xrefs exist.
- `register_emac_int @ 0xff4`: complete and recorded in
  `functions/0x00ff4-register_emac_int.md`; source-like C is in
  `recovered/plat_smac.c`.
- It stores the EPON ISR callback and shared context then returns
  `pon_int_enable(0x100)`. No direct code xrefs exist.
- `register_xeumac_int @ 0x101c`: complete and recorded in
  `functions/0x0101c-register_xeumac_int.md`; source-like C is in
  `recovered/plat_smac.c`.
- It stores the XEPON upstream ISR callback and shared context then returns
  `pon_int_enable(0x200)`. No direct code xrefs exist.
- `register_xedmac_int @ 0x1044`: complete and recorded in
  `functions/0x01044-register_xedmac_int.md`; source-like C is in
  `recovered/plat_smac.c`.
- It stores the XEPON downstream ISR callback and shared context then returns
  `pon_int_enable(0x400)`. No direct code xrefs exist.
- `register_dg_int @ 0x106c`: complete and recorded in
  `functions/0x0106c-register_dg_int.md`; source-like C is in
  `recovered/plat_smac.c`.
- It stores an opaque DGi handler and shared context then returns
  `pon_int_enable(0x20)`. The module does not dispatch that stored handler.
- `register_lp_int @ 0x1094`: complete and recorded in
  `functions/0x01094-register_lp_int.md`; source-like C is in
  `recovered/plat_smac.c`.
- It stores the LP ISR callback and shared context then returns
  `pon_int_enable(0x40)`. No direct code xrefs exist.
- `register_low_power_int @ 0x10bc`: complete and recorded in
  `functions/0x010bc-register_low_power_int.md`; source-like C is in
  `recovered/plat_smac.c`.
- It stores the low-power ISR callback and shared context then returns
  `pon_int_enable(0x800)`. No direct code xrefs exist.
- `register_ptp_int @ 0x10e4`: complete and recorded in
  `functions/0x010e4-register_ptp_int.md`; source-like C is in
  `recovered/plat_smac.c`.
- It stores the PTP ISR callback and shared context then returns
  `nppt_int_enable(0x400)`. No direct code xrefs exist.
- `register_ptp_stamp_int @ 0x110c`: complete and recorded in
  `functions/0x0110c-register_ptp_stamp_int.md`; source-like C is in
  `recovered/plat_smac.c`.
- It stores the PTP timestamp ISR callback and shared context then returns
  `nppt_int_enable(0x200)`. No direct code xrefs exist.
- `register_oam_int @ 0x1134`: complete and recorded in
  `functions/0x01134-register_oam_int.md`; source-like C is in
  `recovered/plat_smac.c`.
- It stores only the OAM ISR callback, leaves shared context unchanged, then
  returns `nppt_int_enable(0x100)`. No direct code xrefs exist.
- `dg_timer_func @ 0x1154`: complete and recorded in
  `functions/0x01154-dg_timer_func.md`; source-like C is in
  `recovered/plat_smac.c`.
- It restores optical TX power, applies four independent PON register updates
  selected by `g_pon_work_mode`, then clears `dg_flag`. `dg_timer_init` stores
  it as the timer callback.
- `epon_set_dg_cnt @ 0x11fc`: complete and recorded in
  `functions/0x011fc-epon_set_dg_cnt.md`; source-like C is in
  `recovered/plat_smac.c`.
- It independently transforms the low nibble at PON offsets `0x1800f0` and
  `0x1c0110` according to EPON/XEPON work-mode bits. Its sole caller is
  `zx_pon_int`.
- `zxic_gpio_set_value @ 0x1258`: complete and recorded in
  `functions/0x01258-zxic_gpio_set_value.md`; source-like C is in
  `recovered/plat_smac.c`.
- It is a one-instruction no-op stub. No direct xrefs or internal argument-type
  evidence exists.
- `epon_get_llid_state @ 0x125c`: complete and recorded in
  `functions/0x0125c-epon_get_llid_state.md`; source-like C is in
  `recovered/plat_smac.c`.
- It returns PON offset `0x180004` bits 8-15; `zx_pon_int` and
  `pon_is_registered` call it.
- `xepon_get_llid_state @ 0x1274`: complete and recorded in
  `functions/0x01274-xepon_get_llid_state.md`; source-like C is in
  `recovered/plat_smac.c`.
- It returns PON offset `0x1c0008` bits 8-15; `zx_pon_int` and
  `pon_is_registered` call it.
- `xgpon_get_onu_state @ 0x128c`: complete and recorded in
  `functions/0x0128c-xgpon_get_onu_state.md`; source-like C is in
  `recovered/plat_smac.c`.
- It returns PON offset `0x59400` low three bits; `zx_pon_int` and
  `pon_is_registered` call it.
- `gpon_get_onu_state @ 0x12a4`: complete and recorded in
  `functions/0x012a4-gpon_get_onu_state.md`; source-like C is in
  `recovered/plat_smac.c`.
- It returns PON offset `0x94000` low three bits; `zx_pon_int` and
  `pon_is_registered` call it.
- `pon_is_registered @ 0x15f4`: complete and recorded in
  `functions/0x015f4-pon_is_registered.md`; source-like C is in
  `recovered/plat_smac.c`.
- It normalizes a nonzero cached flag to one; otherwise evaluates XGPON, GPON,
  EPON, and XEPON registration in independent ordered branches, with later
  matching modes overwriting earlier results. Its sole caller is `cpu_net_tx`.
- `unregister_pon_int @ 0x1780`: complete and recorded in
  `functions/0x01780-unregister_pon_int.md`; source-like C is in
  `recovered/plat_smac.c`.
- It calls `free_irq(g_pon_irq, &pon_int_info)` as a void teardown helper. Its
  sole caller is `zx_pon_remove`.
- `unregister_nppt_int @ 0x17a8`: complete and recorded in
  `functions/0x017a8-unregister_nppt_int.md`; source-like C is in
  `recovered/plat_smac.c`.
- It calls `free_irq(g_nppt_irq, &pon_int_info)` as a void teardown helper. Its
  sole caller is `zx_pon_remove`.
- `apb_write @ 0x17d0`: complete and recorded in
  `functions/0x017d0-apb_write.md`; source-like C is in
  `recovered/plat_smac.c`.
- It stores a 32-bit APB word and returns the unchanged address pointer. No
  direct code xrefs exist.
- `apb_read @ 0x17d8`: complete and recorded in
  `functions/0x017d8-apb_read.md`; source-like C is in
  `recovered/plat_smac.c`.
- It returns one volatile 32-bit APB word. No direct code xrefs exist.
- `apb_bit_write @ 0x17e0`: complete and recorded in
  `functions/0x017e0-apb_bit_write.md`; source-like C is in
  `recovered/plat_smac.c`.
- It dynamically constructs a field mask, ORs an unmasked shifted input value,
  stores the APB word, and returns the unchanged address pointer. Eleven direct
  callers use it.
- `an1_pll_en_cfg @ 0x1808`: complete and recorded in
  `functions/0x01808-an1_pll_en_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears PLL offset `0x10` bit zero, ORs the complete unmasked 32-bit input,
  and returns the updated word. No direct code xrefs exist.
- `serdes_err_cnt_reset @ 0x1824`: complete and recorded in
  `functions/0x01824-serdes_err_cnt_reset.md`; source-like C is in
  `recovered/plat_smac.c`.
- It pulses SerDes offset `0x94` bit 15 low then high with separate rereads.
  Two PRBS counter readers call it.
- `serdes_unlock @ 0x184c`: complete and recorded in
  `functions/0x0184c-serdes_unlock.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears SerDes offset `0x90` bits 13-14 and offset `0x40` bit 15. No direct
  code xrefs exist.
- `an1_pll_en_get @ 0x1870`: complete and recorded in
  `functions/0x01870-an1_pll_en_get.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reads PLL offset `0x10` bit zero, logs and returns the normalized value. No
  direct code xrefs exist.
- `an1_pll_bypass_cfg @ 0x18ac`: complete and recorded in
  `functions/0x018ac-an1_pll_bypass_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- It snapshots seven PLL/SerDes words on every call, then executes mode-zero
  writeback or mode-one/two bypass register sequences. Invalid modes log and
  perform no setup after snapshotting.
- `an1_pll_bypass_get @ 0x1a38`: complete and recorded in
  `functions/0x01a38-an1_pll_bypass_get.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reads PLL offset `0xc` bit seven, logs and returns the normalized bypass
  enable state. No direct code xrefs exist.
- `an1_pll_out_mode_cfg @ 0x1a74`: complete and recorded in
  `functions/0x01a74-an1_pll_out_mode_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears PLL offset `0x1c` bit one, ORs the unmasked input shifted by one,
  and returns explanatory logging status. No direct code xrefs exist.
- `an1_pll_out_mode_get @ 0x1aa8`: complete and recorded in
  `functions/0x01aa8-an1_pll_out_mode_get.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reads PLL offset `0x1c` bit one, logs 156.25 or 155.52 MHz, and returns the
  normalized bit. No direct code xrefs exist.
- `an1_pll_cfg_ring_circle_bisa_set @ 0x1af0`: complete and recorded in
  `functions/0x01af0-an1_pll_cfg_ring_circle_bisa_set.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears PLL offset `0x4` bits 16-19, ORs an unmasked input shifted by 16,
  and returns logging status. No direct code xrefs exist.
- `an1_pll_cfg_ring_circle_bisa_get @ 0x1b28`: complete and recorded in
  `functions/0x01b28-an1_pll_cfg_ring_circle_bisa_get.md`; source-like C is in
  `recovered/plat_smac.c`.
- It extracts PLL offset `0x4` bits 16-19, logs and returns the four-bit value.
  No direct code xrefs exist.
- `an1_pll_cfg_ring_circle_resl_set @ 0x1b64`: complete and recorded in
  `functions/0x01b64-an1_pll_cfg_ring_circle_resl_set.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears PLL offset `0x4` bits 23-26, ORs an unmasked input shifted by 23,
  and returns logging status. No direct code xrefs exist.
- `an1_pll_cfg_ring_circle_resl_get @ 0x1b9c`: complete and recorded in
  `functions/0x01b9c-an1_pll_cfg_ring_circle_resl_get.md`; source-like C is in
  `recovered/plat_smac.c`.
- It extracts PLL offset `0x4` bits 23-26, logs and returns the four-bit value.
  No direct code xrefs exist.
- `com_pll_cfg_ring_circle_bisa_set @ 0x1bd8`: complete and recorded in
  `functions/0x01bd8-com_pll_cfg_ring_circle_bisa_set.md`; source-like C is in
  `recovered/plat_smac.c`.
- It replaces common SerDes offset `0x4` bits 16-19 using an unmasked input,
  prints the full value mapping, and returns the data-log result.
- `com_pll_cfg_ring_circle_bisa_get @ 0x1c28`: complete and recorded in
  `functions/0x01c28-com_pll_cfg_ring_circle_bisa_get.md`; source-like C is in
  `recovered/plat_smac.c`.
- It extracts common SerDes offset `0x4` bits 16-19, logs the value and full
  mapping, then returns the four-bit value.
- `com_pll_cfg_ring_circle_resl_set @ 0x1c70`: complete and recorded in
  `functions/0x01c70-com_pll_cfg_ring_circle_resl_set.md`; source-like C is in
  `recovered/plat_smac.c`.
- It replaces common SerDes offset `0x4` bits 23-26 using an unmasked input and
  returns logging status. No direct code xrefs exist.
- `com_pll_cfg_ring_circle_resl_get @ 0x1ca8`: complete and recorded in
  `functions/0x01ca8-com_pll_cfg_ring_circle_resl_get.md`; source-like C is in
  `recovered/plat_smac.c`.
- It extracts common SerDes offset `0x4` bits 23-26, logs using the binary's
  `an1_`-prefixed string, and returns the four-bit value.
- `serdes_set_tx_swin @ 0x1ce4`: complete and recorded in
  `functions/0x01ce4-serdes_set_tx_swin.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears SerDes offset `0x20` bits 16-17, ORs an unmasked input shifted by
  16, and returns logging status. No direct code xrefs exist.
- `serdes_set_low_power @ 0x1d1c`: complete and recorded in
  `functions/0x01d1c-serdes_set_low_power.md`; source-like C is in
  `recovered/plat_smac.c`.
- It programs the low byte at SerDes offset `0x5c`: modes 0-4 select `0x00`,
  `0xff`, `0xdd`, `0x22`, and `0x33`; mode 5 and values above 5 take distinct
  no-write error-log paths. Every path returns `printk` status.
- `serdes_set_band @ 0x1e28`: complete and recorded in
  `functions/0x01e28-serdes_set_band.md`; source-like C is in
  `recovered/plat_smac.c`.
- It sequentially replaces SerDes offset `0x6c` bit 14 and bits 0-7 using
  unmasked inputs, then returns logging status. No direct xrefs exist.
- `serdes_get_band @ 0x1e6c`: complete and recorded in
  `functions/0x01e6c-serdes_get_band.md`; source-like C is in
  `recovered/plat_smac.c`.
- It extracts SerDes offset `0xd0` bits 16-23, logs, and returns the byte. It
  does not read the `0x6c` register written by `serdes_set_band`; preserve this
  binary-evidenced asymmetry.
- `serdes_set_gen_en @ 0x1ea8`: complete and recorded in
  `functions/0x01ea8-serdes_set_gen_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- It replaces SerDes offset `0x94` bit 13 using an unmasked input and returns
  logging status. `serdes_set_tx_prbs_mode` calls it with 1 and 0.
- `serdes_set_check_en @ 0x1ee0`: complete and recorded in
  `functions/0x01ee0-serdes_set_check_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- It replaces SerDes offset `0x94` bit 14 using an unmasked input and returns
  logging status. RX-BIST callers forward an enable or pass 1.
- `serdes_set_err_cnt_en @ 0x1f18`: complete and recorded in
  `functions/0x01f18-serdes_set_err_cnt_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- It replaces SerDes offset `0x94` bit 15 using an unmasked input and returns
  logging status. RX-BIST callers forward an enable or pass 1.
- `serdes_get_err_cnt @ 0x1f50`: complete and recorded in
  `functions/0x01f50-serdes_get_err_cnt.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reads offset `0xe8` as count bits 0-31, then offset `0xec` bits 0-15 as
  count bits 32-47, logs, and returns the unsigned 48-bit value in `u64`.
- `serdes_prbs_err_ok @ 0x1f98`: complete and recorded in
  `functions/0x01f98-serdes_prbs_err_ok.md`; source-like C is in
  `recovered/plat_smac.c`.
- It sets SerDes offset `0x48` bit 23 and returns logging status. It is a
  command, not a predicate; the log suggests one-bit PRBS error injection.
- `serdes_set_error_time @ 0x1fc8`: complete and recorded in
  `functions/0x01fc8-serdes_set_error_time.md`; source-like C is in
  `recovered/plat_smac.c`.
- Modes 0-4 multiply seconds by 156250000; modes 5-7 use 155520000; other
  modes use zero. A 32-bit product is written to offset `0x98`, and offset
  `0xa4` bits 24-31 are cleared, explicitly truncating the apparent 40-bit
  duration before readback and logging.
- `serdesPrbsCounterGetHandler @ 0x2048`: complete and recorded in
  `functions/0x02048-serdesPrbsCounterGetHandler.md`; source-like C is in
  `recovered/plat_smac.c`.
- It is a void timer callback. It logs overflow if the current PRBS count is
  below `serdesPrbsCounter`; otherwise it logs their difference. It neither
  updates the baseline nor rearms the timer.
- `serdes_set_loopback_mode @ 0x208c`: complete and recorded in
  `functions/0x0208c-serdes_set_loopback_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It snapshots SerDes offsets `0x1c`, `0x24`, `0x40`, `0x48`, `0x90`, and
  `0x94` on its first counted call and restores them before every later call.
  Modes 0-8 apply ordered loopback transactions; mode 9 leaves the restored
  defaults; mode 10 logs an error but increments the counter; values above 10
  return logging status without incrementing. `prbs_enable == 1` sets offset
  `0x94` bits 13-15 and bit 31 after each configuring mode.
- `serdes_set_rx_eq_mbf @ 0x2874`: complete and recorded in
  `functions/0x02874-serdes_set_rx_eq_mbf.md`; source-like C is in
  `recovered/plat_smac.c`.
- It replaces SerDes offset `0x2c` bits 18-21 using an unmasked input and
  returns logging status. No direct xrefs exist.
- `serdes_get_rx_eq @ 0x28ac`: complete and recorded in
  `functions/0x028ac-serdes_get_rx_eq.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reads offset `0x2c` once, reports active-low EQ enable bits 0-2, five-bit
  data fields at bits 3-7, 8-12, and 13-17, and MBF bits 18-21. It returns the
  final MBF `printk` result, not a field value.
- `serdes_set_np_jittery @ 0x2970`: complete and recorded in
  `functions/0x02970-serdes_set_np_jittery.md`; source-like C is in
  `recovered/plat_smac.c`.
- It replaces SerDes offset `0x48` bits 6-8 using an unmasked input and returns
  logging status. No direct xrefs exist.
- `serdes_get_np_jittery @ 0x29a8`: complete and recorded in
  `functions/0x029a8-serdes_get_np_jittery.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reads SerDes offset `0x48` once, extracts bits 6-8, logs, and returns the
  three-bit value. The apparent second read in Hex-Rays is not in assembly.
- `check_serdes_version @ 0x29e4`: complete and recorded in
  `functions/0x029e4-check_serdes_version.md`; source-like C is in
  `recovered/plat_smac.c`.
- It reads offsets `0x4` and `0x18` up front. Selector `0x4[4:1] == 1` means
  V1; selector zero plus `0x18[31:16] == 0x0ef0` or `0x00ff` means V2 or V3;
  all other values log error. The return is always logging status.
- `check_serdes_config @ 0x2a58`: complete and recorded in
  `functions/0x02a58-check_serdes_config.md`; source-like C is in
  `recovered/plat_smac.c`.
- It prints a `pon_serdes_mode` label, dumps 74 words from SerDes offsets
  `0x0..0x124`, and for CPU 132 dumps 10 PLL words from `0x0..0x24`. Its two
  exits retain unrelated residual values, so the recovered semantic ABI is
  void rather than a status return.
- `serdes_set_tx_prbs_mode @ 0x2bf0`: complete and recorded in
  `functions/0x02bf0-serdes_set_tx_prbs_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It always enables the generator, applies first-match CPU 132/133/129 setup,
  and maps modes 0-4 to PRBS7/23/31/9/15. Mode 5 disables the generator and
  writes a fixed `0101` pattern across offsets `0x9c..0xa4`. Invalid modes still
  receive common setup. Every input returns zero.
- `serdes_set_rx_prbs_mode @ 0x2de8`: complete and recorded in
  `functions/0x02de8-serdes_set_rx_prbs_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It applies first-match CPU 132/133/129 setup, then maps modes 0-4 to
  PRBS7/23/31/9/15 via offset `0x94` bits 19-21. Invalid modes retain common
  setup. Every input returns zero; callers pass mode-minus-one or a raw mode.
- `serdes_set_sprbsrxbist @ 0x2f90`: complete and recorded in
  `functions/0x02f90-serdes_set_sprbsrxbist.md`; source-like C is in
  `recovered/plat_smac.c`.
- It passes wrapping 32-bit `prbs_mode - 1` to RX PRBS selection, forwards one
  enable to the checker and error counter in order, discards all helper
  results, and returns its final formatted `printk` result.
- `serdes_set_pattern @ 0x2fe4`: complete and recorded in
  `functions/0x02fe4-serdes_set_pattern.md`; source-like C is in
  `recovered/plat_smac.c`.
- It clears `0x94[15:12]`, applies CPU-133/129 setup, writes an 80-bit pattern
  across offsets `0x9c..0xa4`, and sets `0xa4[18:16]=7` only for enable 1,
  otherwise zero. The residual base pointer is not semantic; ABI is void.
- `check_serdes_lock @ 0x3098`: complete and recorded in
  `functions/0x03098-check_serdes_lock.md`; source-like C is in
  `recovered/plat_smac.c`.
- It independently tests CPU 132, 133, and 129 and prints raw PLL/CDR/ALOS
  bits. CPU 132 includes PLL-block `0x20[0]`; CPU 133 uses SerDes `0xd0[0]`;
  CPU 129 uses `0xcc[1]`; all use two independent reads of `0xe4[1:0]`.
  Residual return values are incoherent, so the semantic ABI is void.
- `get_all_efuse @ 0x3160`: complete and recorded in
  `functions/0x03160-get_all_efuse.md`; source-like C is in
  `recovered/plat_smac.c`.
- `efuse_base @ 0x27680` is the only global it touches. It is declared in
  `plat_smac.c` as `extern volatile uint8_t *efuse_base;` with the
  `EFUSE_U32(offset)` accessor beside the other base-pointer macros.
- One `isCpuType_129()` test selects between two different efuse decode
  layouts, not merely two format strings. Both dump 33 raw words at
  `0x00..0x80` labeled `0x14f11000..0x14f11080`; 129 uses `%#x` and the other
  branch uses `%#u`.
- Layout divergence to preserve: on 129, `0x30`-`0x3c` is the HUK, `0x40`-`0x5c`
  is an 8-word HASH key, `0x60`/`0x64` is the anti-rollback counter, `0x68` is
  `chip_status`, `0x6c` is the reserved-protect word, and `0x70` holds
  `T_TRIM`/`POR`/`IDDQ_CP`/`Chip_Type`. On other CPUs, `0x30`-`0x3c` is the
  backup AES key, `0x40` is the reserved-protect word, and `0x44`-`0x50` hold
  the process/trim fields including `PON_mode` at `0x48[31:30]`.
- `crc_ate_ft1` and `system_time` are stitched from two independent 32-bit
  loads. `system_time` is `EXTR W1, W0, W1, #0x18`, that is
  `(word_0x10 << 8) | (word_0x0c >> 24)`. Hex-Rays shows it as an unaligned
  `*(_QWORD *)(efuse_base + 12) >> 24`; keep the two-load form instead.
- It never caches a word across `printk` calls. Verified census: 129 branch has
  83 `printk` and 98 word reads, the other branch 92 and 99, plus one shared
  tail call, so 176 machine calls against 177 in C. Per-offset multiplicities
  match exactly. Preserve every read.
- Useful cross-check technique for these dump routines: count `BL printk` and
  `LDR W<n>, [X0,#imm]` per branch range and compare against the accessor
  expansions in the reconstruction. Base-pointer loads alone undercount,
  because multi-word fields reuse one base load for several offset loads.
- Security-relevant vendor behavior: it logs the raw AES secret key, and either
  the HUK and HASH key or the backup AES key, to the kernel log.
- Both branches converge on `MOV W0, #0` at `0x40ec`, so the ABI is a real
  `int get_all_efuse(void)` that always returns 0. No direct xrefs exist.
- `serdes_set_tx_eq @ 0x410c`: complete and recorded in
  `functions/0x0410c-serdes_set_tx_eq.md`; source-like C is in
  `recovered/plat_smac.c`.
- Its real ABI is `int serdes_set_tx_eq(uint32_t tx_eq)`, with a constant zero
  return. It accepts exactly 0 or 1: value 0 writes `0x0d` and value 1 writes
  `0x1d` into `pon_serdes_base + 0x20[15:8]`, preserving the other bits by
  `& 0xffff00ffU`. Other inputs read nothing, write nothing, log nothing, and
  still return zero.
- The two vendor log strings intentionally contain leading and trailing
  newlines. There are no IDB xrefs, but `vendor-reference/.../system/proc/kallsyms`
  carries both `__ksymtab_serdes_set_tx_eq` and a global `serdes_set_tx_eq`
  symbol, so retain its exported-API status.
- `serdes_set_pll_open_loop @ 0x417c`: complete and recorded in
  `functions/0x0417c-serdes_set_pll_open_loop.md`; source-like C is in
  `recovered/plat_smac.c`.
- It is another exported, xref-free diagnostic/control API. It opens only when
  `enable == 1`; every other value closes. Both paths first read SerDes `0x68`,
  then update `0x68[10:9]`, reread and update `0x68[22]`, then update
  `0x74[13]`. Preserve the two separate `0x68` RMW operations and their
  ordering.
- Unlike `serdes_set_tx_eq`, it returns the final `printk` result. Its vendor
  kallsyms evidence includes `__ksymtab_serdes_set_pll_open_loop` and the
  global `serdes_set_pll_open_loop` symbol.
- `serdes_set_clk_change @ 0x4200`: complete and recorded in
  `functions/0x04200-serdes_set_clk_change.md`; source-like C is in
  `recovered/plat_smac.c`.
- It is an exported, xref-free RX-to-TX clock-source selector. Zero clears
  `pon_serdes_base+0x48[18]` and logs the vendor's `looptiming` spelling; every
  nonzero value sets the bit for local clock. It performs one RMW and returns
  the final `printk` result.
- `serdes_set_rx_eq1 @ 0x424c`: complete and recorded in
  `functions/0x0424c-serdes_set_rx_eq1.md`; source-like C is in
  `recovered/plat_smac.c`.
- Valid `enable` values are exactly 0 and 1; larger unsigned values only log an
  error. Bit `0x2c[0]` is active-low: zero disables EQ1 by setting it, one
  enables it by clearing it. The latter path then independently rereads `0x2c`
  and replaces bits `7:3` by ORing an **unmasked** `equalizer_value << 3`.
  Preserve both RMW operations.
- Returns in the machine code are incoherent (printk result, written word, or
  residual base pointer), so retain the semantic `void` ABI. Kallsyms confirms
  it is exported via `__ksymtab_serdes_set_rx_eq1`.
- `serdes_set_rx_eq2 @ 0x42b0`: complete and recorded in
  `functions/0x042b0-serdes_set_rx_eq2.md`; source-like C is in
  `recovered/plat_smac.c`.
- It repeats EQ1's validation and residual-return structure for EQ2: bit
  `0x2c[1]` is active-low, and enabling performs two ordered RMWs before
  replacing destination bits `12:8` with an **unmasked**
  `equalizer_value << 8`. Retain the semantic `void` ABI and its exported
  status from `__ksymtab_serdes_set_rx_eq2`.
- `serdes_set_rx_eq3 @ 0x4314`: complete and recorded in
  `functions/0x04314-serdes_set_rx_eq3.md`; source-like C is in
  `recovered/plat_smac.c`.
- It completes the EQ1/EQ2/EQ3 triplet: `0x2c[2]` is active-low and the data
  field is `0x2c[17:13]`. Values above 1 only log; enabling preserves the two
  ordered RMWs and ORs an **unmasked** `equalizer_value << 13`. Its residual
  returns remain semantically void; kallsyms shows `__ksymtab_serdes_set_rx_eq3`.
- `serdes_set_lane_mode @ 0x4378`: complete and recorded in
  `functions/0x04378-serdes_set_lane_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- It affects only CPU type 133. It clears `pon_serdes_base+0x94[2:0]`, ORs an
  **unmasked** `lane_mode`, then writes back; every other CPU type has no MMIO
  effect. Predicate/masked-value residual returns make it semantically void.
  It is exported through `__ksymtab_serdes_set_lane_mode`.
- `serdes_set_error_time_en @ 0x43b8`: complete and recorded in
  `functions/0x043b8-serdes_set_error_time_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- Module-private helper called by `serdes_get_hard_prbs_cnt(1)` and
  `serdes_get_prbs_counters(0)`. It replaces `0x94[30]` by clearing it then
  ORing an **unmasked** `enable << 30`, logs, and returns the `printk` result.
  Do not conflate it with the distinct exported `uni_` variant.
- `serdes_get_hard_prbs_cnt @ 0x43ec`: complete and recorded in
  `functions/0x043ec-serdes_get_hard_prbs_cnt.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported synchronous measurement API: reset error counter; set `seconds` and
  RX PRBS mode; enable check/error/error-time controls in order; then loop
  `((uint32_t)(seconds * 1000U))` times, incrementing before each
  `__const_udelay(0x418958UL)`. It adds the resulting 64-bit count to global
  `iPrbsCounter @ 0x27870` and returns the final `printk` result.
- The loop has no scheduling, timeout, lock, or atomic increment. Preserve its
  32-bit multiplication truncation and global read-add-write behavior.
- `serdes_get_prbs_counters @ 0x4490`: complete and recorded in
  `functions/0x04490-serdes_get_prbs_counters.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported timer-based PRBS setup. It always logs and disables error-time first.
  A nonzero `los_state_prbs` logs RXBIST-unlocked and returns 0. Otherwise it
  does `del_timer`, `init_timer_key(...serdesPrbsCounterGetHandler...)`, then
  writes `serdes_prbs_counter_timer.expires` (the qword at `0x27888`) as
  `jiffies + (uint32_t)(time * 100U)`, adds the timer, resets, delays once, and
  stores the 64-bit baseline in `serdesPrbsCounter`.
- Preserve the 32-bit wrapping time multiplication, all helper order, and the
  unconditional zero return. The binary offers no locking or explicit timer
  synchronization beyond `del_timer`.
- `mode_epon_cfg @ 0x4560`: complete and recorded in
  `functions/0x04560-mode_epon_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Mode-0 EPON profile script, called only by `serdes_mode_set` case 0. It logs,
  checks CPU 132 first, then CPU 133 only if 132 did not match. Each supported
  branch performs 46 ordered 32-bit writes from offsets `0x00..0xb4`, then the
  shared 32-bit tail `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0`. Unsupported CPUs
  receive no writes and do not execute the common tail.
- All values are raw hardware profile constants. Preserve individual store
  width/order; do not replace the shared final three writes with a 64-bit store.
  The residual pointer/predicate returns make its semantic ABI void.
- `mode_10g_epon_nsyn_dpll_cfg @ 0x4908`: complete and recorded in
  `functions/0x04908-mode_10g_epon_nsyn_dpll_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Mode-1 10G EPON non-synchronous DPLL profile. It has the same CPU-132-first,
  CPU-133-fallback shape as `mode_epon_cfg`, with 46 branch-specific ordered
  32-bit stores at `0x00..0xb4`, then `0xb8=0x80`, `0xbc=0x10000`, and
  `0xc0=0`. Unsupported CPUs skip all profile and common-tail stores.
- `serdes_mode_set` case 1 passes two generic arguments, but this entry never
  reads them. Keep the semantic `void(void)` ABI and direct 32-bit store order;
  do not let Hex-Rays' wide-tail rendering merge the last two writes.
- `mode_10g_epon_nsyn_fifo_cfg @ 0x4cc4`: complete and recorded in
  `functions/0x04cc4-mode_10g_epon_nsyn_fifo_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Mode-2 10G EPON non-synchronous FIFO profile. Same CPU-132-first,
  CPU-133-fallback structure: 46 raw ordered 32-bit stores at `0x00..0xb4`,
  then the supported-CPU-only tail `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0`.
  `serdes_mode_set` case 2 passes arguments this profile ignores, so retain the
  semantic `void(void)` ABI and individual tail writes.
- `mode_10g_epon_nsyn_nofifo_cfg @ 0x5080`: complete and recorded in
  `functions/0x05080-mode_10g_epon_nsyn_nofifo_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Mode-3 10G EPON non-synchronous no-FIFO profile. Again CPU 132 wins before
  the CPU-133 fallback; a matching CPU receives 46 direct 32-bit stores at
  `0x00..0xb4` plus `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0`. It ignores the
  dispatcher's generic parameters, has a semantic `void(void)` ABI, and must
  retain each tail store separately.
- `mode_10g_epon_syn_cfg @ 0x543c`: complete and recorded in
  `functions/0x0543c-mode_10g_epon_syn_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Mode-4 synchronous 10G EPON profile is CPU-133-only. A match receives 49
  ordered raw 32-bit stores at `0x00..0xc0`; a nonmatch only logs. It ignores
  dispatcher arguments and has a semantic `void(void)` ABI. Preserve its three
  final stores, particularly the separate `0xbc` and `0xc0` writes.
- `mode_gpon_cfg @ 0x5644`: complete and recorded in
  `functions/0x05644-mode_gpon_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Mode-5 GPON script selects CPU 129 first, then 132, then 133. Each supported
  CPU receives 46 raw 32-bit profile words plus the common tail
  `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0`. CPU 129's profile-specific tail is
  `0xac=0x201c`, `0xb0=0x0c`, `0xb4=0x01000000`; CPU 132 uses `0x0d/0/0`; CPU
  133 uses `0x40002000/0x0c/0x01000000`.
- The 129 and 133 tails share instructions but not values. Preserve resulting
  explicit stores and the CPU priority. The generic dispatcher arguments remain
  unused and the semantic ABI is `void(void)`.
- `mode_gpon_syn_cfg @ 0x5ba8`: complete and recorded in
  `functions/0x05ba8-mode_gpon_syn_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- CPU-129-only synchronous GPON profile. It deliberately logs the inherited
  string `mode_gpon_cfg`, then writes 49 direct 32-bit words at `0x00..0xc0`
  only if 129 matches. It has no direct IDB xrefs, no vendor ksymtab export,
  ignores dispatcher arguments, and retains a semantic `void(void)` ABI.
- `mode_xgpon_nsyn_cfg @ 0x5d90`: complete and recorded in
  `functions/0x05d90-mode_xgpon_nsyn_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Mode-6 non-synchronous XGPON profile selects CPU 132 first, then CPU 133.
  CPU 132 gets 44 direct profile stores plus the common three-word tail, but
  retains separate CPU-133 predicates before writing `0x20=0x8f000000` and
  `0x2c=0xaa8`. Since the helpers compare the same global to distinct values,
  those two writes are normally skipped on an unchanged CPU-132 state; do not
  collapse them into unconditional profile words. CPU 133 gets 46 profile
  stores and the same `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0` tail. Generic
  dispatcher arguments are unused and the semantic ABI is `void(void)`.
- `mode_xgpon_syn_cfg @ 0x6174`: complete and recorded in
  `functions/0x06174-mode_xgpon_syn_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Mode-7 synchronous XGPON profile is CPU-133-only. It writes all 49 ordered
  32-bit words at `0x00..0xc0`; a one-time byte read of `product_vid` selects
  alternatives at `0x20`, `0x48`, and `0x80` when any raw test
  `vid&0x7f==6`, `vid&0xfd==0xa4`, `vid==0x63`, `vid==0x97`, or
  `vid&0xfb==1` succeeds. Preserve the byte-width read, exact tests, explicit
  tail writes, ignored dispatcher arguments, and semantic `void(void)` ABI.
- `eth_an1_clk_set @ 0x63ec`: complete and recorded in
  `functions/0x063ec-eth_an1_clk_set.md`; source-like C is in
  `recovered/plat_smac.c`.
- Ethernet AN1 PLL profile writes eight 32-bit values at offsets `0x00..0x1c`,
  then RMWs bit zero at `+0x10`. It polls `+0x20` bit zero with a counter of
  1001 and `__const_udelay(0x8312b0)` between failed reads. Both lock success
  and timeout return the final completion `printk` status; timeout first emits
  its failure log. `an1_pll_clk_set` selects it for modes 8-16.
- `an1_pll_epon_cfg @ 0x64c0`: complete and recorded in
  `functions/0x064c0-an1_pll_epon_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- EPON AN1 PLL entry has the same eight profile writes and `+0x10` bit-zero RMW
  as the Ethernet entry, but uses `__const_udelay(0x418958)` for up to 1001
  failed `+0x20` bit-zero polls and EPON-specific logs. It returns the final
  completion `printk` result and is selected by `an1_pll_clk_set` for modes
  0-4.
- `an1_pll_gpon_cfg @ 0x6594`: complete and recorded in
  `functions/0x06594-an1_pll_gpon_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- GPON AN1 PLL entry differs from the Ethernet/EPON profile only at
  `+0x08=0x01050700` and `+0x1c=0x00130000`. It RMWs `+0x10` bit zero, performs
  up to 1001 `+0x20` bit-zero polls with `__const_udelay(0x8312b0)`, logs GPON
  lock status, and returns the final completion `printk` result. It is selected
  for modes 5-7.
- `mode_eth_10gbase_r_cfg @ 0x6664`: complete and recorded in
  `functions/0x06664-mode_eth_10gbase_r_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- The shared Mode-13/14 Ethernet 10GBASE-R/10G USXGMII profile selects CPU 132
  first and CPU 133 as fallback. Each supported branch performs 46 direct raw
  32-bit stores at `0x00..0xb4`, then `0xb8=0x80`, `0xbc=0x10000`, and
  `0xc0=0`. Generic dispatcher arguments are unused and the semantic ABI is
  `void(void)`; preserve the individual tail store widths.
- `mode_eth_5gbase_r_cfg @ 0x6a28`: complete and recorded in
  `functions/0x06a28-mode_eth_5gbase_r_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- The shared Mode-11/12 Ethernet 5GBASE-R profile also selects CPU 132 before
  CPU 133 and writes 46 raw 32-bit profile words plus the common
  `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0` tail. Mode 12 is labeled
  `MODE_ETH_USXGMII_5G`; `check_serdes_config` does not give mode 11 its own
  label. Preserve this observed dispatcher/diagnostic mismatch and its semantic
  `void(void)` ABI.
- `mode_eth_2p5gbase_r_cfg @ 0x6df0`: complete and recorded in
  `functions/0x06df0-mode_eth_2p5gbase_r_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Mode-10 Ethernet 2.5GBASE-R selects CPU 132 first, then CPU 133, then CPU
  129 only when 133 did not match. CPU 132 has one 46-word profile; CPU 133 and
  129 share another 46-word profile. Both append direct tail stores
  `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0`. Keep the 133-before-129 test order,
  unused dispatcher arguments, and `void(void)` semantic ABI.
- `mode_eth_2p5gbase_x_cfg @ 0x71c8`: complete and recorded in
  `functions/0x071c8-mode_eth_2p5gbase_x_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Shared Mode-9/15 Ethernet 2.5GBASE-X entry selects CPU 132, then 133, then
  129 and gives each a distinct 46-word raw profile followed by
  `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0`. Cases 9 and 15 are diagnosed as
  `MODE_ETH_HSGMII` and `MODE_ETH_2P5BASE_X`; retain the shared implementation,
  profile priority, individual tail widths, ignored dispatcher arguments, and
  semantic `void(void)` ABI.
- `mode_eth_1gbase_x_cfg @ 0x7728`: complete and recorded in
  `functions/0x07728-mode_eth_1gbase_x_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Shared Mode-8/16 Ethernet 1GBASE-X entry selects CPU 132, then 133, then 129
  and gives each a distinct 46-word raw profile plus the tail
  `0xb8=0x80`, `0xbc=0x10000`, `0xc0=0`. Cases 8 and 16 are diagnosed as
  `MODE_ETH_SGMII` and `MODE_ETH_1GBASE_X`; retain the shared implementation,
  profile priority, individual tail widths, ignored dispatcher arguments, and
  semantic `void(void)` ABI.
- `an1_pll_clk_set @ 0x7c74`: complete and recorded in
  `functions/0x07c74-an1_pll_clk_set.md`; source-like C is in
  `recovered/plat_smac.c`.
- CPU-132-only caller `pon_serdes_init` uses this mode dispatcher for AN1 PLL
  setup. Its bounded raw bit-map selects EPON for 0-4 (`0x1f`), GPON for 5-7
  (`0xe0`), and Ethernet for 8-16 (`0x1ff00`), propagating the selected child's
  `int` result. Values above 16 call no child and retain the selector residual.
- `serdes_mode_set @ 0x7cc0`: complete and recorded in
  `functions/0x07cc0-serdes_mode_set.md`; source-like C is in
  `recovered/plat_smac.c`.
- This is the central unsigned mode 0-16 jump-table dispatcher for every PON
  and Ethernet profile. Cases 8/16, 9/15, 11/12, and 13/14 intentionally share
  callees; values above 16 are no-ops. The generic second argument is unused by
  every profile body, and the sole caller ignores all incidental register
  residuals, so retain `void serdes_mode_set(u32 mode)`.
- `pon_serdes_init @ 0x7d58`: complete and recorded in
  `functions/0x07d58-pon_serdes_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- This mode-init orchestrator calls AN1 setup only on CPU 132, always dispatches
  the selected SerDes profile, then RMWs `+0x90` bit 13, `+0x40` bit 15, and
  `+0x54` bit 0. CPU 132/133 poll `+0xd0` bit 0 for common PLL lock; CPU 129
  polls `+0xcc` bit 1. Every CPU then reports `+0xe4` LOS and polls its bit 1
  CDR lock; CPU 129 additionally waits until `+0xe4` bit 9 or 10 is set. Each
  loop begins with 1001 retries and uses `__const_udelay(0x8312b0)`; any timeout
  returns `-1`, otherwise the function returns zero.
- `pon_pll_cfg @ 0x7f20`: complete and recorded in
  `functions/0x07f20-pon_pll_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Modes 0-4, 5-7, and 8-16 choose EPON, GPON, and Ethernet CRM PLL transactions
  respectively; other modes are no-ops. CPU 132 receives dedicated EPON/GPON
  literals. Preserve non-132 EPON/GPON's `+0xc4` bit-28 RMW before its literal
  overwrite, every valid path's final `+0xc` set-bit-9/clear-bit-8 RMWs, and its
  constant-zero `int` result.
- `zx_pon_clk_reset_init @ 0x8088`: complete and recorded in
  `functions/0x08088-zx_pon_clk_reset_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported top-level PON clock/reset entry passes mode to `pon_pll_cfg`, stores
  `pon_serdes_mode`, then uses CPU-specific CRM pulses. CPU 129/133 toggle
  `+0x70` bits 0 and 1 around ten `__const_udelay(0x418958)` calls per phase;
  CPU 132 additionally toggles `+0x60` bit 9 between the `+0x70` releases.
  All paths call `pon_serdes_init(mode)` and only log its status. Preserve the
  raw zero return as semantic `void(void)`.
- `uni_apb_write @ 0x829c`: complete and recorded in
  `functions/0x0829c-uni_apb_write.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported APB helper is exactly one volatile 32-bit store and returns the input
  address unchanged. It has no internal IDB xrefs; retain
  `volatile u32 *uni_apb_write(volatile u32 *address, u32 value)` for external
  module callers.
- `uni_apb_read @ 0x82a4`: complete and recorded in
  `functions/0x082a4-uni_apb_read.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported APB read helper is exactly one volatile 32-bit load returning an
  unsigned zero-extended value. It also has no internal IDB xrefs; retain
  `u32 uni_apb_read(const volatile u32 *address)` for external module callers.
- `uni_apb_bit_write @ 0x82ac`: complete and recorded in
  `functions/0x082ac-uni_apb_bit_write.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported APB field helper performs one volatile load, clears
  `((1 << width) - 1) << shift`, ORs the unmasked `value << shift`, writes the
  result, and returns its address. It has no internal IDB xrefs; do not add a
  value mask that is absent from the binary.
- `uni_serdes_err_cnt_reset @ 0x82d4`: complete and recorded in
  `functions/0x082d4-uni_serdes_err_cnt_reset.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported Uni SerDes error-counter pulse performs two RMWs at `+0x94`: clear
  bit 15 then set it. It returns zero and is called before counter reads by both
  Uni SerDes PRBS getter APIs.
- `uni_serdes_set_pattern @ 0x82fc`: complete and recorded in
  `functions/0x082fc-uni_serdes_set_pattern.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported pattern helper clears `+0x94` bits 12-15, writes two pattern words at
  `+0x9c/+0xa0`, replaces only `+0xa4` low 16 bits, then sets bits 16-18 only
  for an exact enable value of one or clears them otherwise. It returns the
  final `+0xa4` word and has no internal IDB xrefs.
- `zx_uni_clk_reset_init @ 0x834c`: complete and recorded in
  `functions/0x0834c-zx_uni_clk_reset_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported Uni clock/reset API is a two-instruction successful no-op returning
  zero. It has no internal IDB xrefs; retain its `int(void)` ABI for external
  callers.
- `uni_com_pll_cfg_ring_circle_bisa_set @ 0x8354`: complete and recorded in
  `functions/0x08354-uni_com_pll_cfg_ring_circle_bisa_set.md`; source-like C is
  in `recovered/plat_smac.c`.
- Exported Uni common-PLL setter clears `+0x04` bits 16-19, then ORs the raw
  unmasked `value << 16`; do not truncate input to four bits. It emits its
  mapping log and returns the second `printk` result. It has no internal IDB
  xrefs.
- `uni_com_pll_cfg_ring_circle_bisa_get @ 0x83a4`: complete and recorded in
  `functions/0x083a4-uni_com_pll_cfg_ring_circle_bisa_get.md`; source-like C is
  in `recovered/plat_smac.c`.
- Exported paired getter reads `uni_serdes_base + 0x04`, extracts bits 16-19,
  logs the value and mapping, then returns the unsigned four-bit field. Its only
  current xref is export metadata.
- `uni_com_pll_cfg_ring_circle_resl_set @ 0x83ec`: complete and recorded in
  `functions/0x083ec-uni_com_pll_cfg_ring_circle_resl_set.md`; source-like C is
  in `recovered/plat_smac.c`.
- Exported paired ring-circle R setter clears `+0x04` bits 23-26, then ORs raw
  unmasked `value << 23`. Do not constrain the input to four bits; it logs and
  returns the final `printk` status, and its only current xref is export
  metadata.
- `uni_com_pll_cfg_ring_circle_resl_get @ 0x8424`: complete and recorded in
  `functions/0x08424-uni_com_pll_cfg_ring_circle_resl_get.md`; source-like C is
  in `recovered/plat_smac.c`.
- Exported paired ring-circle R getter reads `uni_serdes_base + 0x04`, extracts
  bits 23-26, logs, and returns the unsigned four-bit field. It has no internal
  IDB xrefs.
- `uni_serdes_set_tx_swin @ 0x8460`: complete and recorded in
  `functions/0x08460-uni_serdes_set_tx_swin.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported Uni SerDes TX SWIN setter clears `+0x20` bits 16-17, then ORs raw
  unmasked `value << 16`. Do not constrain the input to two bits; it logs and
  returns the final `printk` result and has no internal IDB xrefs.
- `uni_serdes_set_low_power @ 0x8498`: complete and recorded in
  `functions/0x08498-uni_serdes_set_low_power.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported low-power selector controls the low byte of `uni_serdes_base + 0x5c`:
  modes 0-4 clear, OR `0xff`, or replace it with `0xdd`, `0x22`, or `0x33`.
  Mode 5 and values above 5 both avoid MMIO but log distinct errors. Every path
  returns its `printk` result.
- `uni_serdes_set_band @ 0x85a4`: complete and recorded in
  `functions/0x085a4-uni_serdes_set_band.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported PLL-band setter does two ordered `+0x6c` RMWs: clear bit 14 then OR
  raw `band_select << 14`, then clear the low byte and OR raw `band_value`. Do
  not constrain either input to its apparent field width; the function returns
  the completion `printk` result and has no internal IDB xrefs.
- `uni_serdes_get_band @ 0x85e8`: complete and recorded in
  `functions/0x085e8-uni_serdes_get_band.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported Uni SerDes band reader extracts bits 16-23 from `+0xd0`, logs and
  returns the byte. It is not the inverse of `uni_serdes_set_band`, which writes
  `+0x6c`; preserve the different register addresses and no-internal-xref
  context.
- `uni_serdes_set_gen_en @ 0x8624`: complete and recorded in
  `functions/0x08624-uni_serdes_set_gen_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported Uni SerDes PRBS generator setter clears `+0x94` bit 13, then ORs raw
  unmasked `value << 13`. Do not constrain input to one bit; it returns its
  final `printk` result and is used by `uni_serdes_set_tx_prbs_mode`.
- `uni_serdes_set_check_en @ 0x865c`: complete and recorded in
  `functions/0x0865c-uni_serdes_set_check_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported Uni SerDes PRBS checker setter clears `+0x94` bit 14, then ORs raw
  unmasked `value << 14`. It returns its final `printk` result and is used by
  RX BIST and hard PRBS counter routines.
- `uni_serdes_set_err_cnt_en @ 0x8694`: complete and recorded in
  `functions/0x08694-uni_serdes_set_err_cnt_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported Uni SerDes PRBS error-counter setter clears `+0x94` bit 15, then ORs
  raw unmasked `value << 15`. It returns its final `printk` result and is used
  by RX BIST and hard PRBS counter routines.
- `uni_serdes_get_err_cnt @ 0x86cc`: complete and recorded in
  `functions/0x086cc-uni_serdes_get_err_cnt.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported Uni SerDes error-counter getter combines full `+0xe8` low 32 bits
  with only `+0xec` low 16 bits in positions 32-47, then logs and returns the
  48-bit result. It is used by timer, hard-counter, and snapshot paths.
- `uni_serdes_prbs_err_ok @ 0x8714`: complete and recorded in
  `functions/0x08714-uni_serdes_prbs_err_ok.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported PRBS error-OK helper RMW-sets `uni_serdes_base + 0x48` bit 23, logs
  the action, and returns the final `printk` result. It has no internal IDB
  xrefs.
- `uni_serdes_set_error_time_en @ 0x8744`: complete and recorded in
  `functions/0x08744-uni_serdes_set_error_time_en.md`; source-like C is in
  `recovered/plat_smac.c`.
- Despite its Uni name, exported error-time setter targets `pon_serdes_base +
  0x94`: it clears bit 30 then ORs raw unmasked `value << 30`. It returns its
  final `printk` result and is used by both Uni PRBS counter readers.
- `uni_serdes_set_error_time @ 0x8778`: complete and recorded in
  `functions/0x08778-uni_serdes_set_error_time.md`; source-like C is in
  `recovered/plat_smac.c`.
- Error-time count uses `uni_serdes_mode`: modes 0-4 multiply raw units by
  156250000, modes 5-7 and 17 by 155520000, all other modes write zero to
  `+0x98`. The function then clears `+0xa4` bits 24-31 before logging its
  composed count; preserve the order and its final `printk` return.
- `uni_serdesPrbsCounterGetHandler @ 0x87fc`: complete and recorded in
  `functions/0x087fc-uni_serdesPrbsCounterGetHandler.md`; source-like C is in
  `recovered/plat_smac.c`.
- This Uni error-counter reporting handler reads the current 48-bit counter and
  logs either an unsigned difference from `uni_serdesPrbsCounter` or an overflow
  error when the current value is lower. It does not mutate the baseline; its
  two current xrefs are data references in `uni_serdes_get_prbs_counters`.
- `uni_serdes_set_rx_eq_mbf @ 0x8840`: complete and recorded in
  `functions/0x08840-uni_serdes_set_rx_eq_mbf.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported Uni SerDes RX EQ MBF setter clears `+0x2c` bits 18-21, then ORs raw
  unmasked `value << 18`. Do not constrain input to four bits; it returns the
  final `printk` result and has no internal IDB xrefs.
- `uni_serdes_get_rx_eq @ 0x8878`: complete and recorded in
  `functions/0x08878-uni_serdes_get_rx_eq.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported RX EQ diagnostic takes one `+0x2c` snapshot. Bits 0-2 signal that
  EQ1/2/3 is disabled when set; enabled values occupy bits 3-7, 8-12, and
  13-17. It always logs MBF bits 18-21 and returns the final `printk` result.
- `uni_serdes_set_np_jittery @ 0x893c`: complete and recorded in
  `functions/0x0893c-uni_serdes_set_np_jittery.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported Uni SerDes NP jitter setter clears `+0x48` bits 6-8, then ORs raw
  unmasked `value << 6`. Do not constrain input to three bits; it returns the
  final `printk` result and has no internal IDB xrefs.
- `uni_serdes_get_np_jittery @ 0x8974`: complete and recorded in
  `functions/0x08974-uni_serdes_get_np_jittery.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported paired NP jitter getter reads `uni_serdes_base + 0x48`, extracts
  bits 6-8, logs, and returns the unsigned three-bit value. It has no internal
  IDB xrefs.
- `pin_mux_debug_clk_133_out0 @ 0x89b0`: complete and recorded in
  `functions/0x089b0-pin_mux_debug_clk_133_out0.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported debug-clock output transaction sets CRM `+0x1d0` bit 4, performs
  raw unmasked pin-mux low-bit and CRM bits 16-18, 20-21, and 0-3 RMWs, then
  logs the observed fields. Preserve every store and separate diagnostic CRM
  reads; no internal IDB xrefs target it.
- `pin_mux_debug_clk_133_out1 @ 0x8a68`: complete and recorded in
  `functions/0x08a68-pin_mux_debug_clk_133_out1.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported debug-clock output 1 sets CRM `+0x1d0` bit 12, performs raw
  unmasked pin-mux bits 12-14 and CRM bits 24-26/8-11 RMWs, then logs separate
  diagnostic reads. Preserve every store and its final `printk` return; no
  internal IDB xrefs target it.
- `uni_check_serdes_config @ 0x8b08`: complete and recorded in
  `functions/0x08b08-uni_check_serdes_config.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported Uni SerDes diagnostic maps mode 0/1/3-8 to Ethernet labels; mode 2
  intentionally shares the default error path. It prints 74 raw 32-bit words
  from `+0x00..0x124` with fixed log addresses `0x16100000..0x16100124`, then
  returns the separator `printk` result. Preserve the full loop and no-xref
  context.
- `uni_serdes_set_loopback_mode @ 0x8c00`: complete and recorded in
  `functions/0x08c00-uni_serdes_set_loopback_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- This exported CPU-133 loopback state machine saves `+0x1c/+0x24/+0x40/+0x48/
  +0x60/+0x90/+0x94` once and restores them before every later call. Valid modes
  0-10 advance and return a persistent counter; modes 0-8 apply distinct,
  ordered RMW profiles only on CPU 133, mode 9 retains restored defaults, and
  mode 10 logs a profile error. Out-of-range inputs return the bounds-error log
  status without incrementing. Exact auxiliary value one adds `+0x94` masks
  `0xe000` and `0x80000000` for profile modes 0-8.
- `uni_serdes_set_tx_prbs_mode @ 0x941c`: complete and recorded in
  `functions/0x0941c-uni_serdes_set_tx_prbs_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported TX PRBS selector always calls `uni_serdes_set_gen_en(1)`. CPU 133
  and CPU 129 use distinct preparatory `+0x24/+0x94` profiles; then selector 0,
  1, or 2 programs `+0x94` bits 16-18 for PRBS7, PRBS23, or PRBS31. Other
  selectors leave that field unchanged. Every path returns zero.
- `uni_serdes_set_rx_prbs_mode @ 0x9560`: complete and recorded in
  `functions/0x09560-uni_serdes_set_rx_prbs_mode.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported RX PRBS selector has distinct CPU-133/129 setup at `+0x48/+0x94`,
  then maps selector 0/1/2 to PRBS7/23/31 through `+0x94` bits 19-21. Vendor
  logs still say `tx`; unsupported selectors leave the field unchanged and all
  paths return zero.
- `uni_serdes_set_sprbsrxbist @ 0x96a8`: complete and recorded in
  `functions/0x096a8-uni_serdes_set_sprbsrxbist.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported RX BIST state machine snapshots `+0x24/+0x48/+0x94` once. Exact
  enable value one invokes the RX selector/check/error-counter setters; every
  other value restores defaults, including the observed asymmetric `+0x24` to
  `+0x14` write. Each call increments and returns its persistent counter.
- `uni_check_serdes_lock @ 0x9784`: complete and recorded in
  `functions/0x09784-uni_check_serdes_lock.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported lock diagnostic reads PLL status from CPU-129 `+0xcc` bit 1 or other
  CPUs’ `+0xd0` bit 0, then separately reads CDR bit 1 and ALOS bit 0 at
  `+0xe4`. It logs and returns the final `printk` result; no internal IDB xrefs.
- `uni_serdes_get_hard_prbs_cnt @ 0x97dc`: complete and recorded in
  `functions/0x097dc-uni_serdes_get_hard_prbs_cnt.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported hard PRBS counter path resets/configures RX PRBS, uses a 32-bit
  `time_units * 1000` delay count with `__const_udelay(0x418958)`, then adds the
  48-bit error count to persistent `uni_iPrbsCounter` and returns the final
  `printk` result.
- `uni_serdes_get_prbs_counters @ 0x9880`: complete and recorded in
  `functions/0x09880-uni_serdes_get_prbs_counters.md`; source-like C is in
  `recovered/plat_smac.c`.
- The exported Uni PRBS snapshot path retains an otherwise-unused volatile read
  of `uni_serdes_base + 0xe4`, reinitializes a distinct
  `uni_serdes_prbs_counter_timer`, and writes `jiffies + ((uint32_t)time * 100)`
  to its `+0x10` expiry field before taking a fresh 48-bit baseline.
- `uni_serdes_reset @ 0x9940`: complete and recorded in
  `functions/0x09940-uni_serdes_reset.md`; source-like C is in
  `recovered/plat_smac.c`.
- This exported command accepts only `uni`/`enable` values zero or one. Lane
  zero uses base bit 12 and `+0x400` bits 8/9; lane one uses `+0x200` bit 12 and
  `+0x400` bits 6/7. Enable sequences preserve the long `0x8312b0` then short
  `0x418958` delays; disable sequences perform the inverse ordered clears.
- The reconstructed `void` return is a strong ABI inference. Machine X0 is
  merely left with the most recent callee result, and the module supplies no
  call site establishing a meaningful result contract.
- `uni_serdes_set_tx_eq @ 0x9ae8`: complete and recorded in
  `functions/0x09ae8-uni_serdes_set_tx_eq.md`; source-like C is in
  `recovered/plat_smac.c`.
- The exported Uni TX-EQ setter accepts only zero or one, replacing
  `uni_serdes_base + 0x20` bits 15:8 with `0x0d` or `0x1d`. Other values make
  no volatile access or log; every path returns zero.
- `uni_serdes_set_pll_open_loop @ 0x9b58`: complete and recorded in
  `functions/0x09b58-uni_serdes_set_pll_open_loop.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exact input one sets Uni SerDes `+0x68` bits 10:9 and 22 plus `+0x74` bit 13;
  all other values clear them. The initial `+0x68` read supplies the first
  write, while a second volatile read produces the bit-22 write; retain both.
- `uni_serdes_set_clk_change @ 0x9bdc`: complete and recorded in
  `functions/0x09bdc-uni_serdes_set_clk_change.md`; source-like C is in
  `recovered/plat_smac.c`.
- Zero clears Uni SerDes `+0x48` bit 18 and all nonzero values set it. Each
  branch performs a distinct volatile RMW and returns the final vendor log
  result; do not assign stronger clock-source semantics than the log supports.
- `uni_serdes_set_rx_eq1 @ 0x9c28`: complete and recorded in
  `functions/0x09c28-uni_serdes_set_rx_eq1.md`; source-like C is in
  `recovered/plat_smac.c`.
- EQ1 at `uni_serdes_base + 0x2c`: inputs above one log/no-op; zero sets the
  disable bit 0; one performs separate RMWs to clear bit 0 then replace bits
  3-7 with an unmasked `equalizer_value << 3`. The semantic return is `void`.
- `uni_serdes_set_rx_eq2 @ 0x9c8c`: complete and recorded in
  `functions/0x09c8c-uni_serdes_set_rx_eq2.md`; source-like C is in
  `recovered/plat_smac.c`.
- EQ2 uses the same `+0x2c` pattern: inputs above one log/no-op; zero sets
  disable bit 1; one separately clears bit 1 then replaces bits 8-12 with
  unmasked `equalizer_value << 8`. The semantic return is `void`.
- `uni_serdes_set_rx_eq3 @ 0x9cf0`: complete and recorded in
  `functions/0x09cf0-uni_serdes_set_rx_eq3.md`; source-like C is in
  `recovered/plat_smac.c`.
- EQ3 uses the same `+0x2c` pattern: inputs above one log/no-op; zero sets
  disable bit 2; one separately clears bit 2 then replaces bits 13-17 with
  unmasked `equalizer_value << 13`. The semantic return is `void`.
- `uni_mode_eth_10gbase_r_cfg @ 0x9d54`: complete and recorded in
  `functions/0x09d54-uni_mode_eth_10gbase_r_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Internal Uni 10GBASE-R setup logs, then writes 49 ordered 32-bit literals to
  `uni_serdes_base + 0x00..0xc0` with no CPU gate or register reads. It matches
  the recovered CPU-133 10G profile, is selected for Uni modes zero/one, and
  must preserve separate `+0xa0/+0xa4` and `+0xbc/+0xc0` stores.
- `uni_mode_eth_5gbase_r_cfg @ 0x9f54`: complete and recorded in
  `functions/0x09f54-uni_mode_eth_5gbase_r_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Internal Uni 5GBASE-R setup logs, then writes 49 ordered 32-bit literals to
  `uni_serdes_base + 0x00..0xc0` with no CPU gate or register reads. It matches
  the recovered CPU-133 5G profile, is selected for Uni modes two/three, and
  must preserve separate `+0xa0/+0xa4` and `+0xbc/+0xc0` stores.
- `uni_mode_eth_2p5gbase_r_cfg @ 0xa154`: complete and recorded in
  `functions/0x0a154-uni_mode_eth_2p5gbase_r_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Internal Uni 2.5GBASE-R setup logs, then writes 49 ordered 32-bit literals to
  `uni_serdes_base + 0x00..0xc0` with no CPU gate or register reads. It matches
  the recovered non-132 2.5G profile, is selected for Uni mode four, and must
  preserve separate `+0xa0/+0xa4` and `+0xbc/+0xc0` stores.
- `uni_mode_eth_2p5gbase_x_cfg @ 0xa354`: complete and recorded in
  `functions/0x0a354-uni_mode_eth_2p5gbase_x_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Uni 2.5GBASE-X first checks CPU 133, otherwise CPU 129. Each supported path
  writes 49 ordered 32-bit literals at `+0x00..+0xc0`; their `+0xac` word is
  `0x40002000` and `0x0000201c`, respectively. Unsupported CPUs only log.
- `uni_mode_eth_1gbase_x_cfg @ 0xa6fc`: complete and recorded in
  `functions/0x0a6fc-uni_mode_eth_1gbase_x_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Uni 1GBASE-X first checks CPU 133, otherwise CPU 129. Each supported path
  writes 49 ordered 32-bit literals at `+0x00..+0xc0`; their `+0xac` word is
  `0x40002000` and `0x0000201c`, respectively. Unsupported CPUs only log.
- `uni_serdes_mode_set @ 0xaa90`: complete and recorded in
  `functions/0x0aa90-uni_serdes_mode_set.md`; source-like C is in
  `recovered/plat_smac.c`.
- The Uni dispatcher maps modes 0/1 to 10GBASE-R, 2/3 to 5GBASE-R, 4 to
  2.5GBASE-R, 5/6 to 2.5GBASE-X, and 7/8 to 1GBASE-X. Unsigned values above
  eight are no-ops; the sole direct caller is `uni_zx_serdes_init @ 0xaae8`.
- `uni_zx_serdes_init @ 0xaae8`: complete and recorded in
  `functions/0x0aae8-uni_zx_serdes_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- Uni bring-up dispatches its mode, RMW-sets `+0x54` bit 0, and uses 1001
  `__const_udelay(0x8312b0)` polls for the selected PLL status and CDR bit.
  CPU-133 modes 5/6 require `+0xcc[2:0] == 7`; other 133 modes use `+0xd0[0]`;
  non-133 uses `+0xcc[1]`. CPU 129 additionally waits for either `+0xe4[9]` or
  `+0xe4[10]`, preserving two volatile reads per test. All timeouts return -1.
- `uni_pll_cfg @ 0xacb0`: complete and recorded in
  `functions/0x0acb0-uni_pll_cfg.md`; source-like C is in
  `recovered/plat_smac.c`.
- Uni PLL CRM configuration handles mode 0-4 and mode 5-7 with distinct
  literals at `top_crm_base + 0xc0/+0xc4`, preserving separate RMWs of `+0xc4`
  bit 28 and `+0x0c` bits 9/8. Other modes make no CRM access; all paths return
  zero.
- `uni_eth_mode_change @ 0xada8`: complete and recorded in
  `functions/0x0ada8-uni_eth_mode_change.md`; source-like C is in
  `recovered/plat_smac.c`.
- The internal unsigned mapper converts Uni modes 0-8 to clock-reset modes
  `13,14,11,12,10,9,15,8,16`; all other inputs log and return zero. Its sole
  caller invokes `zx_pon_clk_reset_init` only for nonzero mapped values.
- `uni_serdes_init @ 0xae34`: complete and recorded in
  `functions/0x0ae34-uni_serdes_init.md`; source-like C is in
  `recovered/plat_smac.c`.
- Exported Uni wrapper: xmac zero stores the mode, pulses `top_crm_base + 0x70`
  bits 4/5 with ten `__const_udelay(0x418958)` calls between each phase, calls
  `uni_zx_serdes_init`, logs success/failure, and still returns zero. XMAC one
  maps a mode into PON clock reset only when `g_ponserdes_to_xmac1 == 1`.
- `uni_serdes_on_133 @ 0xaf44`: complete and recorded in
  `functions/0x0af44-uni_serdes_on_133.md`; source-like C is in
  `recovered/plat_smac.c`.
- The local helper performs a single volatile RMW to set `uni_serdes_base +
  0x54` bit 0, returns the post-write word, and does not itself test CPU type.

## Latest Continuation

- `net_tst_tx @ 0xe004`: complete and recorded in
  `functions/0x0e004-net_tst_tx.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It rejects null data, zero length, and ports above three. For valid input it
  indexes the four contiguous CPU-netdev slots at `0x28158`, allocates an skb of
  `length + 16` with raw flags `0xa20`, copies the data, sets the skb device, and
  delegates ownership to `cpu_net_tx`. Allocation failure increments stats
  `+0x38` when available but still returns zero.
- `oam_tx @ 0xe0c0`: complete and recorded in
  `functions/0x0e0c0-oam_tx.md`; it is the global fixed-port-two wrapper around
  `net_tst_tx` and propagates that helper's 32-bit status.
- `net_omci_tx_test @ 0xe0d8`: complete and recorded in
  `functions/0x0e0d8-net_omci_tx_test.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- This global diagnostic routine allocates `length + 10`, logs/fills a byte
  ramp, calls the same port-two helper, then frees the temporary source buffer.
  It has no coherent machine return contract, so its semantic ABI is void.
- `dump_net_int_info @ 0xe524`: complete and recorded in
  `functions/0x0e524-dump_net_int_info.md`; source-like C is in
  `recovered/plat_cpu_net.c`.
- It checks an unsigned NAPI-context index against three, then reads and logs
  five adjacent counters at `int_info + 0x1a0 * source + 0x18c..0x19c`. The
  first two are directly updated by `cpu_net_int`; retain the vendor-derived
  labels `poll`, `rx int`, and `tx int` for the other three rather than claiming
  their producers. The semantic ABI is void.
- `sub_E5C8 @ 0xe5c8` and `__fswab32 @ 0xe5e0`: complete and recorded in their
  respective function files; source-like C is in `recovered/plat_cpu_net.c`.
- The former writes input PMR, barriers, writes PMR readback XOR `0xe0`, reads
  TPIDR_EL2, and physically falls through to the latter's `REV W0,W0`; preserve
  its unknown purpose and byte-swapped low-input return rather than assigning a
  standard IRQ helper name.
- `virt_to_phys @ 0xe5e8`: complete and recorded in
  `functions/0x0e5e8-virt_to_phys.md`; it selects `kimage_voffset` subtraction
  or low-39-bit-plus-`memstart_addr` arithmetic using the exact
  `vabits_actual` predicate. `memstart_addr` is a 64-bit imported value.
- `sub_FA60 @ 0xfa60`, `testftp_net_report @ 0xfa68`, and
  `testftp_init @ 0xfb6c`: complete and recorded in their respective function
  files; source-like C is in `recovered/plat_cpu_net.c`.
- `sub_FA60` executes PAN clear then set and falls through to the report entry.
  Preserve it as an unreferenced privileged-state wrapper of unknown purpose.
- The report path increments `testftp_cnt`; metadata bit 2 chooses an
  IPv4-shaped header calculation, bit 1 chooses an IPv6-shaped calculation, and
  neither bit increments an unhandled-type counter. Valid paths optionally
  consume `testftp_debug_cnt` and forward task, derived length, RX length, and
  transport pointer to external `ffe_pre_process_zte`; CPU RX ignores the
  result after returning its backing buffer.
- `testftp_init` only writes literal one to `g_speedtesthffenable`; its machine
  x0 residual is not a public return value.
- `buf_fifo_rls @ 0x10414`, `buf_fifo_rls_all @ 0x104c8`,
  `idm_recycle_stats @ 0x104f8`, and `idm_recycle_init @ 0x1063c`: complete and
  recorded in their respective function files; source-like C is in
  `recovered/plat_cpu_tx.c`.
- FIFO drain selection 0/1/2/3 maps to skb/kmem/wifi0/wifi1 per-CPU staging,
  dequeues at most signed `mask + 1` objects, and releases each through the
  existing selection dispatcher. The all-selection wrapper has no direct IDA
  caller.
- Recycle initialization visits possible CPUs to zero raw staging words
  `+0x200/+0x204`, clears four FIFO records, sets each mask to `0xfff`, and
  allocates 4096 pointer entries using raw flags `0xa20`. Allocation failures
  are retained without rollback. Statistics reads/prints all FIFO counters with
  no snapshot lock.
- Timer subsystem `0x11520` through `0x1198c` is complete; source-like C is in
  `recovered/plat_timer.c` and individual records are under `functions/`.
- Timer0 uses raw offsets `+0x04/+0x08/+0x0c/+0x10`; ordinary configuration uses
  `25000000 / hz`, while exported dothz configuration uses `250000000 / rate`.
  The hard IRQ disables the global IRQ and schedules a high tasklet; tasklet
  processing invokes optional `timer0_func`, re-enables the IRQ, and increments
  `timer0_int_cnt`. No hard-handler MMIO acknowledgement is observed.
- `zx_timer_init` maps `zxic,apb-timer0`, `zxic,apb-timer1`, and `zte,lsp0_crm`,
  sets CRM bit 9 from clock select one, and requests `zx timer0` IRQ with no
  cleanup path. Captured runtime did not map/use this vendor timer block.
- `0x11a0c` is `.altinstructions` data which can replace `__raw_readl`'s LDR
  with LDAR; it is not a C function. `soam_init @ 0x11a10` is complete in
  `recovered/plat_nppt.c`: it RMW-sets `nppt_base+0x2c0000` bits 0/1, spins on
  `nppt_base+0x80` bit 1 without timeout, logs, and returns zero.
- NPPT reset/exit group `0x11aac` through `0x11bfc` is complete in
  `recovered/plat_nppt.c`. NPPU reset pulses `nppt_base+0x2c0004` bit 7, TM
  reset pulses bit 8, both with `__const_udelay(1718000)` and literal zero
  return. `nppt_exit` calls `idm_exit` before `smac_del_phy_scan`.
- `arch_local_irq_save_0`/`arch_local_irq_restore_1` are separate DAIF helper
  entries used only by GREG auto-gate accessors; preserve their full DAIF save
  and restore behavior rather than collapsing them with other binary clones.
- GREG control subsystem `0x11c08` through `0x11f84` is complete in
  `recovered/plat_nppt.c`. Probe calls SDET reset, an external delay, PON reset,
  SIPC init, ready poll `(NPPT+0x80 & 0x1fd) == 0x1fd`, then SDET restore only
  on success. The ready poll has exactly 400 delayed failures before `-1`.
- SOPC auto-gate state is NPPT `+0xb8` bit four and is protected only by
  `nppt_glb_auto_gate_lock` plus DAIF save/restore. GREG init programs BP
  size/usable-length registers and fixed SMAC/XMAC runt masks.
- `greg_sdet_share_clk_cfg @ 0x11f84` has verified ABI
  `int (u32 enable)`: only 0/1 RMW NPPT `+0x19c` bit zero; all other unsigned
  inputs log and return `-1`. Corrected its `plat_smac.c` declaration and caller
  to forward `enable` while ignoring the return.
- `smac_del_phy_scan @ 0x1293c` and `nppt_smac_set_rgmii_mode @ 0x12980` are
  complete in `recovered/plat_smac.c`. The former only deletes `phy_timer` when
  NPPT exits. The latter RMWs NPPT `+0x24` with clear mask `0x03800000` and set
  bit `0x00800000`, then calls RGMII CRM clock select zero and GREG RGMII mode
  one before logging. Neither has a semantic return value.
- `test_and_set_bit @ 0x12f88` is complete in `recovered/plat_idm.c`. It selects
  a 64-bit word by `bit >> 6`, uses `bit & 63` for the mask, returns an already
  set fast path immediately, otherwise uses an exclusive OR/store-release retry
  plus DMB and returns the prior bit. Its four direct callers are in `_check_abuf`.
- IDM local ABI clones `0x13008` through `0x1307c` are complete in
  `recovered/plat_idm.c`: byte swap, the same 64-bit `vabits_actual` physical
  conversion formula, TPIDR_EL1 accessor, and DAIF save/restore. Corrected the
  local `memstart_addr` declaration to 64 bits, matching the helper's LDR X
  import access and descriptor-address call paths.
- IDM RX/public-control group `0x130c8` through `0x131f0` is complete in
  `recovered/plat_idm.c`. `idm_rx_update` is the `idm_ops + 0x48` callback: it
  performs `DSB ST`, then writes packed queue/count and normal/jumbo-count words
  to IDM `+0x088` and `+0x100`. Its residual `W0` value is not a semantic return.
- `idm_rx_test` is an exported constant-zero stub and `idm_recv_debug_set` is an
  exported no-op. The RX/TX/OMCI debug setters make raw signed 32-bit stores;
  the Wi-Fi RX setter writes both `idm_rx_debug` and `np1_trap_debug`.
  `idm_set_smct_all_trap` replaces IDM base bit 14 from input bit zero and
  returns zero. The public last-count setters and last-buffer-index get/set APIs
  are raw 32-bit global accessors; pool selectors are normal=0, jumbo=1,
  extral=2, with invalid query returning zero and invalid write doing nothing.
- `idm_stat @ 0x13234` is complete in `recovered/plat_idm.c`: an exported,
  35-line volatile IDM diagnostic dump. It samples fixed counter words at IDM
  offsets `0x180` through `0x228` and prints packed words high-half then
  low-half after one read each. It reads DMA debug state `+0x204` last, takes no
  lock/barrier, and has no evidenced semantic return despite the final `printk`
  result remaining in `W0`. Its only direct module caller is `idm_debug_stat`.
- `idm_debug_stat @ 0x13568` is complete in `recovered/plat_idm.c`. It exports
  a software-counter dump for exactly CPU 0/1, using `idm_status` lanes
  `2*cpu + {0,4,8,12}` for normal and `2*cpu + {1,5,9,13}` for jumbo counters,
  followed by repeat counters at indices 16/17 and `idm_stat`. There is no
  lock/snapshot and no evidenced semantic return.
- `idm_print_bppe @ 0x136bc`, `data_padding @ 0x136dc`, `idm_rls_update @
  0x13740`, `idm_cpu_nb_tx_update @ 0x137c4`, `idm_get_smct_all_trap @
  0x13898`, and `get_order @ 0x1397c` are complete in `recovered/plat_idm.c`.
  Padding conditionally zero-fills a raw skb tail but always writes length 60;
  it relies on short-packet callers for nonnegative length. Release update is
  CPU 129/133 gated with repeated probes; non-buffer TX update issues `DSB ST`
  and uses its unchecked dynamic register formula. The SMCT getter validates
  output pointer/null logging and reads IDM bit 14. `get_order` preserves
  unsigned zero-size wraparound.
- `set_idm_int_cpu_rx_cpu_config @ 0x139a4` is complete in
  `recovered/plat_idm.c`. Only a two-CPU system changes IRQ0 affinity: exact
  input one selects `cpu_bit_bitmap + 2` (CPU1), all other inputs select
  `cpu_bit_bitmap + 1` (CPU0), and values mirror to `g_idm_irq_to_cpu` plus
  `eth_xmit_mode`. IDA's adjacent imported-variable labels must not be read as
  queue-count arguments to `irq_set_affinity_hint`.
- `do_raw_spin_lock_flags.isra.1.constprop.21 @ 0x13a2c` is complete under the
  source name `do_raw_spin_lock_flags_idm_lock_int`. It has no argument, uses
  write prefetch plus LDAXR/STXR retry to acquire `idm_lock_int`, and passes a
  contended observed word to `queued_spin_lock_slowpath`. `idm_int_enable` and
  `idm_int_disable` were corrected to call it without the unused DAIF value.
- Refill helper group `0x13bd8` through `0x13d4c` is complete in
  `recovered/plat_idm.c`. `do_raw_spin_lock_1` is the generic pointer qspinlock
  fast path. `idm_rx_refill_flush` drains current-CPU `0x108`-byte staging
  records, with normal words/count at `0x000..0x100` and jumbo at
  `0x080..0x104`; it preserves the verified jumbo wrap against the normal pool
  bound. `idm_rx_refill_reuse` chooses/advances a ring slot under the same lock,
  releases it, then stores the byte-swapped old buffer.
- `idm_alloc_buf @ 0x13df8` is complete in `recovered/plat_idm.c`. A pool-zero
  fast path gated by the exact nonzero `SP_EL0 + 0x10` mask `0xff00` uses a
  current-CPU 32-entry stash (`idm_free_data + 0x100`, count `+0x204`) that
  refills from FIFO0 in at most 32 wrapped-copy entries. Generic unchecked pool
  FIFO pop follows otherwise. Null/empty paths increment diagnostics, then use
  pool-zero `net_alloc_kmem` before `idm_buf_cache`, or `idm_jbuf_cache` for
  nonzero pools; slab flags are `2592`.
- `idm_alloc_nbuf @ 0x14034` is complete in `recovered/plat_idm.c`. It is the
  `idm_ops +0x28` normal allocation callback: calls pool-zero `idm_alloc_buf`,
  classifies the result against the reconstructed reserved data boundary to
  increment `idm_status[2*cpu + 4 or 12]`, then writes only raw header fields
  `+0x00/+0x10/+0x18/+0x28/+0x2c` before returning the buffer.
- `idm_fifo_in @ 0x142b0` is complete in `recovered/plat_idm.c`. It is an
  unchecked-index FIFO producer that adds `0x200` to the current `SP_EL0+0x10`
  word before locking, checks full with `mask + out - in == UINT_MAX`, and
  restores via `__local_bh_enable_ip` on both paths. Success stores/increments
  `in` and per-FIFO input count; full returns `-1` after optional ratelimited
  logging.
- `idm_free_buf @ 0x143c4` is complete in `recovered/plat_idm.c`. It separates
  below-boundary fallback allocations (`idm_status[2*cpu + pool + 8]`, then
  `net_free_kmem` or jumbo cache free) from reserved-side FIFO returns
  (`idm_status[2*cpu + pool]`). Normal high-context returns use a second
  per-CPU stash: producer pointers/count at `+0x000/+0x200`, while the allocator
  consumes the separate `+0x100/+0x204` stash. A full producer batch attempts a
  32-entry FIFO0 copy then clears its local count even after a full-FIFO log.
- `idm_free_skb_data @ 0x14604` is complete in `recovered/plat_idm.c` and
  correctly replaces the prior mistaken data declaration with the function
  pointer assigned to `pp_free_skb_data`. It uses skb `+0x114` bit zero to select
  direct `kfree` versus IDM ownership. Bit 15 drives a raw byte-counted buffer
  table before primary `+0x128` release; both primary and auxiliary addresses
  are boundary-classified to FIFO or normal/jumbo cache paths with status
  updates.
- `dump_tx_desc @ 0x148b4` is complete in `recovered/plat_idm.c`: a void,
  read-only two-line printer for descriptor words `0..6` and named raw fields
  from bytes/halfwords `+0x04/+0x07/+0x0a/+0x18/+0x1a/+0x1b`. It has seven direct
  TX/debug callers and no ownership or state side effects.
- `dump_tx_desc_wifi @ 0x14b8c` is complete in `recovered/plat_idm.c`: a void,
  read-only reduced Wi-Fi diagnostic that prints words `0..6`, `p` from byte 7,
  and length from descriptor halfword `+0x04`. Its only direct caller is
  `idm_wifi_tx`.
- `idm_check_all_tx_desc @ 0x14cd4` is complete in `recovered/plat_idm.c`.
  Valid indices 0..3 select the TX queue's descriptor base and loop up to the
  live `uIDM_TX_QUEUE_DESC_DEPTH`; the verified assembly never advances that
  pointer, so every iteration rechecks only descriptor zero's length field
  `((u16(+4) >> 1) & 0x3fff)`. Short lengths (at most 15) produce a ratelimited
  line plus `dump_tx_desc`. Invalid indices return unchanged; valid calls return
  the final depth. It has no direct module xrefs, locks, barrier, or MMIO write.
- `idm_exit @ 0x15e94` is complete in `recovered/plat_idm.c`: a one-instruction
  empty `RET` hook with no cleanup side effects. Its only direct caller is
  `nppt_exit @ 0x11bcc` at `0x11bd4`.
- `_check_abuf @ 0x15e98` is complete in `recovered/plat_idm.c`. Its low input
  bit selects the normal/jumbo base, stride, refill ring, and pool counts; it
  marks address-derived buffer indices from the selected FIFO, every CPU's
  producer/consumer stash, and a backward refill-ring walk. FIFO scanning uses
  the verified local bottom-half/qspinlock bracket, but stash/ring scans are
  unsynchronized. The `0xdc0` temporary bitmap is deliberately not cleared;
  final accounting scans only `total >> 6` words, has a suspicious
  `0x0fffffff` all-set shortcut, and models the verified 32-bit-shift/
  sign-extended 64-bit mask defect. Normal missing entries print 128 payload
  bytes; jumbo missing entries report unsupported diagnostics. Its direct local
  callers are exported `idm_check_bppe @ 0x16574` and normal-only
  `check_bppe @ 0x16588`.
- `idm_check_bppe @ 0x16574` and `check_bppe @ 0x16588` are complete in
  `recovered/plat_idm.c`. The exported first wrapper forwards a byte unchanged
  to `_check_abuf`; its low bit selects the pool. The local second wrapper always
  passes zero, selecting normal buffers. Neither has an evidenced semantic
  return contract or direct module caller.
- `sub_165A0 @ 0x165a0` is complete in `recovered/plat_nppt.c`. With no incoming
  module xrefs, it reads/discards `TPIDR_EL2`, toggles then restores
  `ICC_PMR_EL1` low bits `0xe0`, issues `DSB SY`, and falls through directly into
  `sipc_init @ 0x165b8`; source models the fall-through as `return sipc_init()`.
- `sipc_init @ 0x165b8` is complete in `recovered/plat_nppt.c`: it writes zero
  to NPPT `+0x4000` and returns zero, with no lock/barrier/poll/error path. Its
  direct callers are `zx_pon_probe`, `nppt_init`, and the fall-through prefix at
  `0x165a0`. `plat_probe.c` now correctly declares its `int` ABI while ignoring
  the result at its existing call site.
- `xmac_eee_conf @ 0x165d0` is complete in `recovered/plat_smac.c`. Exported
  callers select special absolute XMAC windows only for bytes 2/3; all other
  bytes use the NPPT-relative XMAC window. It reads one control word, treats
  only enable byte one as enabled, replaces its `0x000b0000` bits, and writes a
  paired `0x03e80020` or `0x03e80000` timing word. There are no direct module
  xrefs, validation, lock, barrier, or semantic return.
- `xmac0_wan_port_sel @ 0x1677c` is complete in `recovered/plat_smac.c`: exported
  callers have their input truncated to a byte and write that value as a 32-bit
  word to `sys_ctrl_base + 0x0f4`. There is no direct module xref, RMW,
  validation, lock, barrier, or semantic return.
- `xmac_status_show @ 0x16790` is complete in `recovered/plat_smac.c`. Exported
  callers receive a two-XMAC diagnostic based only on globals, not MMIO. The
  function builds an 11 x 64-byte local label table (slots 0..9 populated,
  slot 10 zero), then uses unchecked zero-extended 32-bit
  `sg_xmac_work_mode[0/1]` values and corresponding auto-negotiation bytes to
  print sections. There are no direct module xrefs or persistent mutations.
- `phy_dynamic_identify @ 0x1693c` is complete in `recovered/plat_smac.c`. It
  tests `aal_phy_enable_set` first and writes PHY selector zero when found;
  otherwise it tests `phy_common_c45_enable_set` and writes selector one only
  when that fallback exists. Neither symbol leaves the selector unchanged.
  `xmac_init` is its only direct caller and ignores the residual lookup result.
- `xmac_get_nppt_glb_xpcs_speed_duplex_in_sgmii_mode @ 0x169a0` and its
  USXGMII-named wrapper at `0x169d0` are complete in `recovered/plat_smac.c`.
  The backend has no validation and reads `nppt_base + 0x90 + 4*xmac`, storing
  bits 0..2, 3, and 4 through the three caller pointers. The USXGMII function
  does not access distinct hardware; it forwards all arguments unchanged.
- `sub_16D30 @ 0x16d30` is not an independent recovered C function: it is the
  first tail-dispatch stub of `xmac_get_uni_speed_from_xmac`; its adjacent
  `MOV`/branch entries for output values 1 through 7 are already recorded as
  part of that completed computed-dispatch function.
- `xamc_init_conf @ 0x17034` is complete in `recovered/plat_smac.c`. It follows
  the established special XMAC-2/3 versus NPPT-relative register windows,
  writes four fixed configuration words, then independently reads and ORs the
  final word with `0x200`. Unlike `xamc_init_conf_by_speed`, it does not set
  speed or duplex. It has no direct module caller, validation, lock, barrier,
  or semantic return.
- `xmac_test_siwtch_work_mode @ 0x17a50` is complete in
  `recovered/plat_smac.c`. It maps modes 0..9 to ten exact configuration calls,
  logs their statuses, then enables TX/RX even on child failure and returns that
  child status. Invalid values log the vendor typo and return zero without
  enabling. It is exported but has no direct module caller.
- `xmac_work_mode_switch_to_serdes_mode @ 0x18240` is complete in
  `recovered/plat_smac.c`. Modes 0..8 map to raw SerDes values
  `{0,2,7,7,4,1,3,4,5}` through an unchecked pointer and return zero; any other
  value logs the vendor typo, leaves the output untouched, and returns `-1`.
  It is local and has no direct module caller.
- `xmac_rlt_phy_init @ 0x182c4` is complete in `recovered/plat_smac.c`.
  Despite the local runtime name, it has no observed PHY operation: it calls
  `isCpuType_133()` and only calls `isCpuType_129()` when the first result is
  not exactly one. Its residual return register is discarded by its sole caller,
  `xmac_init`, so the recovered interface is semantic `void`.
- `xmac_mvl_phy_init @ 0x182e4` is complete in `recovered/plat_smac.c`.
  It only gates an early return on CPU-133/129 and `g_xmac0_type == 2`; every
  other path calls `isCpuType_133()` again. There is no observed Marvell PHY
  operation, and its residual return register is discarded by `xmac_init`, so
  the recovered interface is semantic `void`.
- `xmac_bcm_phy_init @ 0x18324` is complete in `recovered/plat_smac.c`. Its
  complete body is one `RET`, with no direct module caller or observable effect.
- `xmac_aqr_phy_init @ 0x18328` is complete in `recovered/plat_smac.c`. It has
  no observed AQR PHY operation: CPU-133 result one returns directly, while all
  other paths call `isCpuType_129()`. Its residual return register is discarded
  by sole caller `xmac_init`, so the recovered interface is semantic `void`.
- `xmac_zxic_phy_init @ 0x18348` is complete in `recovered/plat_smac.c`. It
  gates on raw XMAC type four, scans PHY IDs 4..6, ignores parameter-init status,
  and fills the unexported check table plus seven exported `sg_xphy_*` callback
  tables for every present PHY. Its only direct caller is `xmac_init`, which
  discards the residual register; the recovered interface is semantic `void`.
- `xmac_jl_phy_init @ 0x1845c` is complete in `recovered/plat_smac.c`. Its
  complete body is one `RET`, with no direct module caller or observable effect.
- A full ledger reconciliation found 561 internal code functions. `0x11a0c` is
  ARM64 `.altinstructions` data and `0x16d30` is a tail-dispatch entry already
  recorded with its parent, so neither is independent. Every internal entry is
  now covered by a function record or an explicit non-function/tail note.
- `idm_tx_test @ 0xdd78` is complete in `recovered/plat_cpu_tx.c`. The exported
  six-register test ABI creates a fixed-template skb, maps ports 7/0xffff/other
  to PON/OMCI-OAM/switch devices, and submits requested copies through
  `cpu_net_tx`; unused ABI slots two and three remain intentionally unnamed.
- `hlist_del_init @ 0x10758` is complete in `recovered/plat_cpu_net.c`. It
  unlinks a GRO hlist node only when `pprev` is non-null, repairs the successor,
  and clears both node fields; callers own synchronization and disposal.
- `idm_get_reorder_rls @ 0x138f8` is complete in `recovered/plat_idm.c`. It is
  the IDM ops `+0x58` reader: queues 0/1 return fixed raw words, queue 2 reads
  only on CPU-133/129, and every other selector/path returns zero.
- Exported callback publication and cross-module boundaries are inventoried in
  `CALLBACK_INTERFACES.md`. It distinguishes ksymtab exports from observed
  `np.ko`/`switch.ko` undefined-symbol dependencies and does not infer companion
  callback writes or lifetime behavior from those dependencies.
- `COMPLETENESS.md` records 559 independent function records from 561 internal
  IDA entries (one `.altinstructions` item and one tail-dispatch exclusion), the
  95 imported veneers, syntax-check status for all nine source units, and the
  remaining build blockers.
- Next action: cross-check high-impact reconstructed flows against captured
  runtime dmesg, interrupt counters, and interface state.
