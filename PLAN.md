# ZX279133 Mainline Network Plan

## Current Baseline

- RX page_pool, IRQ/NAPI, 1024-slot TX ring, phylink and ZX279051 PHYLIB are working.
- IPv4, IPv6, VLAN, pause and live MAC change are enabled; the validated CPU-WAN limit remains MTU 1970 with a 1984-byte L2 RX limit.
- The attempted vendor-compatible dual-bank RX extension is not validated: 1944/1954-byte pings pass briefly, but sustained traffic stops all RX after about 2K packets. The source and board are restored to the stable single-bank baseline.
- 10/100 half/full, 1G full and 2.5G full are validated. ZX279051 now caches the raw GE speed code as well as the host interface, so live changes within SGMII rerun the recovered host profile instead of being skipped: the former 1G→100 carrier-without-traffic failure is fixed, two repeated 1G→100→1G cycles passed, and 100 Mbps sustained about 94 Mbit/s bidirectionally. Existing live 1G/2.5G validation remains applicable; 10 Mbps was not retested in this host setup because the Mac service did not establish a 10baseT link.
- 2.5G baseline with doorbell batching and IRQ/app CPU split: RX about 2.35 Gbit/s, TX about 1.94 Gbit/s. The current host topology is WAN on 2.5G USB adapter `en8` (`192.168.1.100`) and LAN on 1G built-in `en0` (`192.168.10.100`); tests must bind the corresponding interface.
- Generic TSO/TSO6 is excluded: the vendor upload path zero-fills payload and cannot carry arbitrary TCP data.
- RX checksum offload is excluded: valid and bad-checksum descriptors are identical.
- Current release candidate FIT `70aaeea8facb4ebce73fcd3ffddf498bde5d2d922576e63e1f44e5a998fa3785` combines Linux-owned ZX279051 cold-reset/fullmask support with the parameter-free RTL8372N DSA production path and no probe/remove diagnostic sweeps. It cold-booted at WAN 2.5G/LAN 1G, passed exact-module reload, and retained zero checked diagnostic info logs and zero driver fault counters.
- Quiescent NPPT global reset/reinitialization is validated for ten complete last-owner teardown/reprepare cycles, alternating LAN-first and WAN-first. Every cycle restored global-done `0x1fd`, reset word `0xffffffff`, XPCS0/1 `0x388b/0x280b` and 2.5G/1G links. This does not validate direct register pulsing or an in-place watchdog reset while DMA users remain active.
- The exact release FIT passed a 30-minute four-flow WAN/LAN full-duplex soak: all four clients completed 1800 one-second intervals without a zero-rate interval, gap or error; 31 board samples retained stable NPPT/XPCS/link state, zero driver/recovery faults and matched release/refill publications. Contention-limited aggregate throughput was about 1.41 Gbit/s, with 21,105 LAN reverse retransmissions explicitly recorded; post-soak LAN-first and WAN-first reprepare both passed.

## LAN / RTL8372N Multi-Port DSA Milestone

- Gate A passed on the live SR1010: MDIO0 `0x14f01000`, switch address `0x1d`, SMI ID register `0x0004` returned `0x83727000` twice.
- `zx279133-rtl8372n.ko` is now a default-enabled, loadable DSA switch driver for the validated SR1010 path: RTL8372N physical port 7 is exposed as `lan1`, CPU port 8 connects to XMAC0, and a dedicated parent-owned `lan-cpu0` conduit keeps WAN `eth0` out of the DSA receive path. The module can now become the first shared NPPT/IDM owner while `eth0` is administratively down.
- The parent demultiplexes LAN ingress to `lan-cpu0`: LAN-only cold start exposes the native XMAC0 source as logical port 5 with VLAN 62 in-band, while concurrent WAN+LAN accepts the SR1010 logical source range 59..62 and inserts/preserves the matching private transport VLAN before DSA dispatch; source 62 remains the live board-validated case. Conduit TX uses NP logical destination port 5. WAN and LAN share one physical IDM TX ring, with BQL attached to that hardware queue while packet/byte/drop accounting remains attributed to the originating netdev.
- The private DSA transport now reserves physical ports 4..7 as VLANs 59..62 and preserves that source sideband in-band through NPPT before DSA dispatch. Switch setup must execute the byte-for-byte validated port7/VLAN62 sequence first, then append ports4..6: a reordered generic loop changed ingress to source 8 with a malformed residual header and broke `lan1`, while adding port4, port5 and port6 after the validated sequence was tested independently and together without port7 regression. The switch driver applies the validated RTL8372N core image, SDS1 mode 13, forced CPU port 8 and PHY7 `page31:a5d0=0`; carrier/speed/duplex are polled for user ports 4..7.
- A bounded proprietary CPU-tag experiment is rejected as an implementation baseline. With RTL8372N protocol-4 tagging enabled, the switch-to-CPU frame arrived at NPPT with descriptor source 0 and only the final four CPU-tag bytes retained (`0000:0007` before an internal VLAN-1 header), proving that the NP parser consumes the leading `0x8899`/protocol/reason word. A custom residual-tag decoder delivered broadcast traffic to `lan1`, but unicast forwarding to CPU port 8 still failed even after the evidenced unknown-unicast mask/action and bounded static-FDB tests. All experimental source was removed; private VLAN transport remains the validated baseline until the complete CPU-tag/FDB contract is recovered.
- Final LAN-first cold-start validation brought `eth0` down before loading the switch module, then registered `lan1` with XPCS0 status `0x388b`. Host `en8` at `192.168.10.100` and board `lan1` at `192.168.10.1` passed IPv4 ping 5/5. Five-second iperf3 measured about 1.48 Gbit/s host-to-board and 1.76 Gbit/s board-to-host with 14 retransmissions; conduit and user-port errors remained zero.
- Device-tree bindings for the NPPT conduit and RTL8372N DSA child pass `dt_binding_check`. The initramfs packages the DSA core, tagger and switch module with working module aliases and `/sbin/modprobe`.
- `ethtool -S lan1` now exposes the four evidence-backed 64-bit RTL8372N MIB counters for MAC-side and PHY-side good TX/RX frames. A cold-boot test showed all four counters increase consistently with LAN traffic; error-counter packing remains intentionally unexposed until its 32-bit pair ordering is validated.
- RTL8372N reset ownership is now explicit: the DT provides the board-validated active-low GPIO17 reset, probe performs a 100 ms assert plus 100 ms post-deassert recovery, and removal or probe failure asserts and holds reset after DSA/TX quiescence. Two consecutive same-boot `rmmod`/`modprobe` cycles rebuilt the switch core and DSA tree, retained XPCS0 status `0x388b`, and passed `lan1` ping 5/5 after each reload. The former reload failure also depended on inheriting U-Boot's VEND2 BMCR; mode-5 setup now writes the complete vendor `0x3140` image before the AN-disable tail leaves `0x2140`.
- Do not read the Uni SerDes MMIO window after its clocks have been disabled; a direct powered-off `devmem` read caused a watchdog reboot during this validation.
- The shared lifecycle is split and owner-tracked: WAN and LAN owner bits are serialized by `datapath_lock`; the RTL8372N child acquires shared hardware before switch setup, publishes conduit readiness only after XMAC0/XPCS0/switch configuration, and requires a successful TX quiesce before restoring hardware or dropping ownership. The first-to-last shared lifetime owns RX/NAPI/IRQ/IDM plus the XPCS runtime-PM reference.
- Board validation confirmed both ownership directions. LAN remained operational after `eth0 down`; starting WAN while LAN owned the datapath relinked `eth0` at 1 Gbit/s, WAN IPv6 link-local ping passed 3/3 and LAN ping remained 5/5. With WAN down again, LAN ping remained 3/3; removing the final LAN owner released shared hardware, and a subsequent `eth0 up` freshly prepared WAN and passed IPv6 ping 3/3 with zero interface errors.
- All four front-panel jacks are now individually mapped and traffic-validated at 2.5 Gbit/s: eth1→physical7/`lan1`, eth2→physical6/`lan2`, eth3→physical5/`lan3`, eth4→physical4/`lan4`. Host-to-board results were about 1.45-1.47 Gbit/s and board-to-host about 1.73-1.75 Gbit/s across the newly tested ports, with bidirectional ping, XPCS0 `0x388b`, and zero conduit/interface errors. Lower copper rates remain deferred.
- Bridge membership and MSTP state handling are implemented, but private transport VLANs 59..62 now remain permanently isolated as `{one user port, CPU8}`. DSA bridge join returns `tx_fwd_offload=false`, and the tagger does not set `offload_fwd_mark`, so Linux owns bridge forwarding and customer-VLAN policy instead of allowing unvalidated direct switch forwarding to bypass it. MSTP instance0 still maps Linux disabled/blocking/listening/learning/forwarding states to hardware states 0/1/1/2/3. The earlier one-port bridge/STP validation remains applicable; direct two-endpoint software bridge forwarding still needs a second independent LAN endpoint.
- DSA FDB add/delete/dump remains board-validated for VLAN-unaware/FID-1 bridges. VLAN-aware customer-VID FDB offload is now deliberately rejected because access traffic traverses the switch under the private transport VID, so an IVL customer-VID LUT entry would falsely claim hardware control of a software-filtered path.
- Untagged customer access VLAN add/delete is complete on all four front-panel ports in FIT `0e1fc535933299539ee98bba398ab9ec9df5a0125bc9ba4fc37c1ab11e98f1c6`: `lan1`/eth1, `lan2`/eth2, `lan3`/eth3 and `lan4`/eth4. The bridge must be created with `vlan_default_pvid 0`; VID 1 and private VIDs 59..62 are reserved. Every port passed VLAN100 PVID/untagged ping 5/5 and same-boot delete/re-add followed by ping 5/5. On `lan1`, direct replacement by VLAN200 was rejected until deletion and tagged/trunk VLAN200 was rejected with `-EOPNOTSUPP`; adding disconnected `lan2` as access VLAN200 did not disturb `lan1`. Five-second VLAN100 throughput was about 1.42/1.29 Gbit/s host-to-board/board-to-host on `lan1`, 1.42/1.28 Gbit/s on `lan2`, 1.43/1.28 Gbit/s on `lan3`, and 1.42/1.27 Gbit/s on `lan4`; XPCS0 stayed `0x388b`, conduit/WAN errors and drops stayed zero, while DSA user ports retained multicast-related RX-drop counts. Tagged/trunk customer VLANs are not implemented.
- RTL8372N production-path cleanup is complete in FIT `70aaeea8facb4ebce73fcd3ffddf498bde5d2d922576e63e1f44e5a998fa3785`: the five Gate-era module parameters were removed and the previously default DSA path is unconditional without changing its register order, waits or rollback. Probe/remove no longer execute the large core/PHY/SDS/MIB/XMAC/XPCS diagnostic sweeps; cold boot and exact-module reload both had no parameter directory, zero checked diagnostic info logs, XPCS0 `0x388b`, LAN/WAN ping, 941/939 and 947/941 Mbit/s LAN throughput, and 2.34 Gbit/s WAN receive. Independent review found no blocker/high/medium issue. Artifacts are under `out/rtl8372n-production-cleanup-20260821/`.

## Active P2 Performance Work

### 1. TX Doorbell Batching

Implementation:

- Accumulate prepared TX descriptors in `tx_notify_pending`.
- Publish `count << 17`, matching vendor `idm_cpu_nb_tx_update()`.
- Use `wmb()` to provide the vendor-required DSB ST before the doorbell.
- Use `__netdev_tx_sent_queue(..., netdev_xmit_more())` so BQL cannot stop an undoorbelled batch.
- Force a flush on ring-full, drop, drain and live release paths.
- Never read completion MMIO after hardware shutdown.

Validated at 1G:

- Board-to-host: 941 Mbit/s, zero retransmissions.
- 652,138 descriptors used 17,963 doorbell writes, averaging 36.3 descriptors per write.
- Host-to-board after interface down/up: 940 Mbit/s.
- Ping, down/up, RX/TX errors and NPPT error all passed.

Completed gate:

- en8 2.5G TX: 1.94 Gbit/s, zero retransmissions, 36.1 descriptors per doorbell.
- en8 controlled RX: 2.35 Gbit/s with IRQ/NAPI on CPU1 and listener on CPU0.
- Errors and NPPT state remained clean; runtime affinity restored.

### 2. Remove Unnecessary `skb_linearize()`

Rejected after controlled test:

- Skipping `skb_linearize()` for already-linear skb built and booted.
- Controlled single-flow TX remained 1.94 Gbit/s with CPU1 about 97.7% busy.
- Four-flow TX reached only 1.86 Gbit/s, so the bottleneck is not a single TCP window.
- Source was reverted to the validated linear TX path.

### 3. NAPI-Driven TX Completion Reclaim

Validated and retained:

- Removed the two unconditional completion MMIO reads from each `start_xmit()` call.
- TX-done IRQ/NAPI owns normal reclaim; xmit polls only as a ring-full fallback.
- 2.5G TX remained 1.94 Gbit/s with zero retransmissions.
- TX CPU dropped from about 97.7% to about 87.9%.
- 1,681,550 descriptors needed about 79,917 completion polls instead of roughly two polls per packet.
- A 20-second run completed 3,359,115 descriptors with no stall, errors or drops.
- Down/up and controlled RX at 2.35 Gbit/s passed.

TX completion reliability completed:

- Normal TX-done IRQ/NAPI reclaim is retained, with a 10 ms delayed-work fallback armed only while descriptors are outstanding. This fixes low-rate completion starvation without restoring per-packet MMIO polling.
- BQL completion is reported once per reclaim/release round for the shared physical queue.
- `ndo_tx_timeout` is implemented for both `eth0` and `lan-cpu0`; it logs ring, completion, doorbell and interrupt state, reclaims completed descriptors, wakes both logical queues, and leaves a true no-progress stall stopped rather than issuing an unverified NPPT reset.
- Cold-boot validation transferred a 29,880-byte TFTP file with `tx_timeouts=0`; the delayed worker reclaimed the low-rate ACK completions that previously caused repeated watchdog timeouts.
- With only IDM TX-done interrupt bit 9 masked, a 10-second UDP run delivered 58,250 datagrams while delayed reclaim handled 58,185 completions, with zero timeout, stall or TX-error counters. After restoring the mask, access-VLAN TCP remained about 1.43 Gbit/s host-to-board and 1.35 Gbit/s board-to-host, and `eth0` down/up plus RTL8372N module reload preserved XPCS0 `0x388b`.

### 4. RX Refill-Starvation Diagnostics and Recovery

Validated and retained:

- `ethtool -S eth0` now separates RX IRQ/NAPI activity, descriptor-not-ready, invalid DMA, page-map misses, jumbo/error-flag drops, page allocation/copy fallback, skb allocation, refill-post failure, release/refill publication, live page-map occupancy and refill-deficit/recovery state.
- The accounting invariant is directly observable: normal operation keeps `rx_release_published == rx_refill_published + rx_refill_deficit`, with `rx_page_map_count=2048`.
- A descriptor that cannot republish a page records a bounded refill deficit. NAPI immediately attempts to allocate/post replacement pages; a 100 ms delayed worker only reschedules NAPI if allocation/posting is still recoverably blocked. Page allocation, page-map mutation, free-ring writes and `IDM_BP_REFILL` publication remain serialized in NAPI context.
- A temporary three-post failure injector, removed from the final source, forced replacement-post failure, original-page repost failure and immediate recovery failure. The delayed retry ran once, restored the missing page, returned `rx_refill_deficit` to zero and preserved 5/5 ping. Counters recorded three post failures, one shortfall, two recovery attempts, one recovery failure and one recovered page.
- After recovery, 1 Gbit/s host-to-board traffic sustained 940 Mbit/s for 10 seconds with the page map restored to 2048 and no further errors or shortfalls.
- The final injection-free FIT sustained 940 Mbit/s WAN RX and 1.41 Gbit/s access-VLAN LAN RX for 10 seconds. Across more than two million final-boot RX completions, invalid DMA, page-map miss, allocation, skb, post, shortfall and deficit counters all remained zero; XPCS0 stayed `0x388b`.
- RX shutdown now disables/quiesces NAPI before disabling page-pool direct recycling, and delayed refill work is synchronously cancelled before page ownership is released.

Fail-closed boundary:

- An unknown descriptor DMA can leave an unidentified stale page-map entry. The driver records and warns about it but does not guess which page to remove or overfill the hardware free ring. Recovery of that class remains an interface/shared-datapath reinitialization problem.

### 5. IDM Interrupt Routing Audit

Validated and retained:

- The vendor IDM information-word split is now reproduced instead of merging every low-16 source onto the direct RX IRQ: `IDM+0x44=0x0000fdff` for direct CPU RX and `IDM+0x50=0x00000200` for the separately named `local-test` source.
- The DT `local-test` IRQ is requested with `IRQF_NO_AUTOEN`, masks only its own bit before scheduling the shared NAPI instance, and is re-enabled with the direct RX source only after NAPI completion. The direct RX IRQ continues to mask only `0xfdff`.
- Both IRQs are synchronously masked and disabled before NAPI/page-pool teardown, and the initramfs pins both GIC IRQ 33 and GIC IRQ 36 to CPU1.
- A final cold boot read back mask/info values `0x07ff0000/0x0000fdff/0x00000200`; 10-second WAN RX remained 940 Mbit/s with zero errors, refill shortfalls or timeout counters.
- Ordinary WAN and LAN traffic did not assert the vendor `local-test` bit-9 source. A bounded RAM-only route of all low-16 sources through the local-test information word proved that GIC IRQ 36 and the handler are live; the deliberately mismatched route produced an expected interrupt storm because the production handler masks only bit 9. Exact production values were restored and ping remained clean.
- The NPPT aggregate IRQ remains unrequested because its acknowledge/status contract is not recovered. The IDM companion/Wi-Fi source remains masked because mainline has no consumer for that path. The buffer-release source remains disabled because page_pool refill does not use the vendor buffer-release callback.

### 6. RX Filtering Policy and Concurrency-Safe Statistics

Validated and retained:

- RX admission remains owned by the fixed NPPT/PPU port-flow image. On the final FIT, 20 raw frames addressed to the board MAC, a foreign unicast MAC and broadcast each advanced `rx_packets`/`rx_irq_count` by exactly 20. Twenty arbitrary multicast frames advanced neither counter.
- `IFF_ALLMULTI` and `IFF_PROMISC` did not make arbitrary multicast reach IDM, while foreign unicast continued to advance the counters in promiscuous mode. A matching IPv4 multicast `dev_mc` membership also did not broaden hardware admission. Protocol-table-selected multicast, including the already validated IPv6 control path, remains supported.
- The driver deliberately omits `ndo_set_rx_mode` and `IFF_UNICAST_FLT`: all unicast is already delivered for Linux software classification, but no evidence-backed runtime multicast-filter programming contract exists.
- WAN and `lan-cpu0` packet/byte counters now use managed per-CPU `pcpu_sw_netstats`; per-netdev errors, drops and length errors use atomic 64-bit counters. RX diagnostic counters use a single-writer `u64_stats_sync` snapshot, IRQ/work counters use atomics, and shared TX diagnostics are read under `tx_lock`.
- Final FIT `91607e058325ade4b04162304b68b1c84ca2c5e54a86506be976fa1fc0a21c76` cold-booted cleanly. During a 10-second 940 Mbit/s WAN RX run, 800 repeated ethtool-stat reads completed without failure, and `ifconfig` exactly matched ethtool packet/byte/error/drop values; release/refill counts remained equal with zero RX faults.
- With `eth0` administratively down and LAN still owning the shared datapath, LAN ping passed 5/5, `lan-cpu0` TX advanced from 23 to 30 packets, and the WAN ethtool view correctly showed shared `idm_tx_done` advance from 5419 to 5426 instead of incorrectly reporting zero. WAN subsequently relinked and passed ping 3/3.

### 7. ZX279051 Reset and Power Lifecycle

Validated and retained:

- Linux again owns SR1010 GPIO1 as the ZX279051 active-low external reset with 100 ms assert/deassert delays. Cold-reset recovery no longer depends on U-Boot state.
- `zx279051_config_init()` replays the vendor `phy_zxic_051_phy_init_fullmask()` contract: 117 ordered writes (3 C22 and 114 direct C45), the exact 200/100/20/30/20 ms power/calibration timing, temperature-status and CFCTRL branches, and the final BMCR state. The table is tied to the audited vendor module SHA-256 and guarded by a 117-entry static assertion.
- Every MDIO operation is checked and fails closed. Because the vendor sequence ends in `BMCR_PDOWN`, `config_init()` explicitly calls and checks `genphy_resume()` before returning; it does not rely on a caller-specific later resume.
- The implementation completed two independent cold boots and multiple actual GPIO reset-backed `eth0` down/up cycles directly at 2.5G/full with en8. A diagnostic generic `phy_init_hw()` and a C22 `BMCR_RESET` plus poll/reinit both returned success, left BMCR at zero and automatically restored 2.5G.
- Exact final FIT `8d2e47ed7b572c52e0f4464837498441f4468c38081d8ffa0c23bb6b73247b0e` cold-booted at 2.5G, then passed another GPIO-low reset/reinitialize cycle. WAN reached 2.35 Gbit/s host-to-board and 1.94 Gbit/s board-to-host with zero retransmits; lan4/en0 remained 1G/full at 940/941 Mbit/s receiver throughput and XPCS0 `0x388b`.
- Final driver diagnostics had zero RX/TX errors or drops, invalid DMA, refill shortfall/deficit, TX timeout or stall; RX release/refill publications matched. Independent review and checkpatch were GO with no blocker or high-severity finding.
- Vendor periodic NBase-T soft-AN and high-BER downshift/recovery remain separate runtime-adaptation work, not a cold-reset dependency. `CONFIG_SUSPEND=n`, so full platform system sleep is still unclaimed.

### 8. VLAN Network-Stack and Stress Matrix

Validated on the final reset/PM FIT with `lan4`/physical port 4 as VLAN100 PVID/untagged access:

- IPv4 and IPv6 link-local ping passed from the host to `br-lan.100`; board-to-host IPv4 and IPv6 probes also received replies.
- IPv4 TCP, 10 seconds: host-to-board held 1.41 Gbit/s in every steady interval and 1.41 Gbit/s aggregate; board-to-host held 1.29-1.31 Gbit/s and 1.30 Gbit/s aggregate with 28 retransmissions.
- IPv4 UDP at a requested 1 Gbit/s: host-to-board delivered 892993/892993 datagrams with zero loss. Board-to-host was sender-limited to 65.4 Mbit/s and lost 1 of 58427 received datagrams (0.0017%).
- IPv6 TCP, 5 seconds: host-to-board held 1.39-1.40 Gbit/s and 1.39 Gbit/s aggregate; board-to-host held 1.26-1.27 Gbit/s and 1.26 Gbit/s aggregate with 5 retransmissions.
- IPv6 UDP at a requested 1 Gbit/s: host-to-board delivered 446450/446450 datagrams with zero loss. Board-to-host was sender-limited to 65.8 Mbit/s and lost 1 of 29319 received datagrams (0.0034%).
- A static PMTU probe with `IP_PMTUDISC_DO` / `IPV6_PMTUDISC_DO` verified exact local limits. At MTU 1400, IPv4 UDP payload 1372 and IPv6 payload 1352 succeeded while the next byte failed with `EMSGSIZE` and reported MTU 1400. After restoring MTU 1500, the corresponding 1472/1452 payloads succeeded and 1473/1453 failed with `EMSGSIZE` and MTU 1500.
- Five-minute simultaneous bidirectional IPv4 TCP completed without iperf or driver errors. Host-to-board intervals ranged 0.694-1.03 Gbit/s (median 0.858, aggregate 0.850); board-to-host ranged 0.194-0.761 Gbit/s (median 0.355, aggregate 0.360) with 134 retransmissions. These rates reflect deliberate two-direction CPU contention and are not substituted for the one-direction line-rate results above.
- After stress, `lan-cpu0` had 27,150,562 RX / 11,837,417 TX packets with zero errors or drops. `lan4` had the same RX count and 1001 software RX drops from the DSA/user-port layer, but no errors; the shared driver reported zero RX/TX errors, invalid DMA, refill shortfalls, refill deficit, TX timeout or pending descriptors, and release/refill publications matched at 27,602,054.
- XPCS0 remained `0x388b`; VLAN100 delete/re-add succeeded and was followed by ping 5/5. Concurrent WAN ping on `eth0` also passed 3/3.

### 9. DMA Ownership and Coherency Normalization

Validated and retained:

- The shared 1024-slot/four-queue CPU-TX descriptor ring is a 128 KiB `dmam_alloc_coherent()` allocation rather than a normal-pointer cast into the no-map IDM reserved region. Failed-drain teardown restores/deconfigures the IDM TX base while clocks are available, so managed memory cannot be freed while hardware still references it.
- All 24 IDM RX descriptor queues (`24 * 2048 * 32` bytes) are a single 1.5 MiB DMA-coherent allocation. Descriptor ownership uses little-endian `READ_ONCE`/`WRITE_ONCE` access with `dma_rmb()` after the ready flag and `dma_wmb()` before ownership/publication.
- The active 4096-entry normal RX free ring is a 16 KiB DMA-coherent big-endian array. Refill posts use `cpu_to_be32()` and retain the existing `dma_wmb()` before the IDM refill doorbell. Unused jumbo and TX-retrieval free-ring registers retain their vendor reserved-memory offsets.
- The driver enforces a 32-bit DMA mask and rejects any coherent allocation crossing 4 GiB. RX page buffers remain owned by page_pool/DMA API and TX payload slots remain DMA coherent.
- Final FIT `64ae64b68ca4a76dbbb00871e17a1f87d02f07e134e5768d0a7ca245b9214e22` cold-booted with IDM TX descriptor base `0x9bc40000`, RX descriptor base `0x9bd00000`, and active RX free-ring base `0x9bc20000`.
- WAN validation reached 940 Mbit/s host-to-board and 941 Mbit/s board-to-host for 10 seconds, with zero retransmissions, interface errors/drops, invalid DMA, page-map misses, refill shortfalls/deficit, TX timeout or pending descriptors. RX release/refill publications matched at 891191 with page-map occupancy 2048.
- Concurrent LAN validation on `lan4` reached 1.45 Gbit/s host-to-board and 1.76 Gbit/s board-to-host for 10 seconds. After 2,414,819 RX completions, release/refill publications still matched, page-map occupancy remained 2048, and all shared RX/TX fault counters remained zero.
- A last-owner teardown followed by LAN-first reprepare restored the same three DMA bases, XPCS0 `0x388b`, page-map occupancy 2048 and zero fault counters; after WAN rejoined, interface-bound WAN ping passed 3/3 and LAN ping passed 5/5. Post-reprepare host-to-board iperf reached 940 Mbit/s on WAN and 1.45 Gbit/s on LAN; 1,035,296 RX release/refill publications matched with zero invalid-DMA, page-map, refill or timeout faults.
- Safety: never read IDM MMIO after the last datapath owner gates `pon_idm_aclk`. An accidental post-release `devmem` read triggered the known watchdog reboot; all final reprepare reads were performed only after the LAN owner had powered the shared datapath again.
- Validation artifacts and a verified checksum manifest are archived under `out/dma-coherency-20260821/`.

### 10. Standard NAPI Weight and RX Queue Fairness

Validated and retained:

- The custom NAPI weight 512 was removed. The driver now uses standard `netif_napi_add()` and kernel `NAPI_POLL_WEIGHT=64`; the cold-boot `netif_napi_add_weight_locked() called with weight 512` error is gone.
- NAPI64 alone preserved individual WAN 940/941 Mbit/s and LAN 1.71-1.81/1.76 Gbit/s operation, but exposed a real fairness defect: every poll restarted at logical queue 7, so simultaneous host-to-board WAN+LAN traffic left LAN at 1.80 Gbit/s while WAN collapsed to about 0.5 Mbit/s.
- RX polling now retains a rotating cursor over the vendor-compatible logical order `7,15,6,14,...,0,8`, pairing each physical queue's normal and jumbo banks while advancing after every empty, partial or budget-limited queue. RX prepare resets the cursor before NAPI is enabled.
- With the rotating cursor, individual host-to-board tests reached WAN 940 Mbit/s and LAN 1.81 Gbit/s. Simultaneous host-to-board tests preserved WAN at 940 Mbit/s while LAN received the remaining 0.88-1.00 Gbit/s instead of starving WAN; simultaneous reverse traffic reached WAN 920 Mbit/s and LAN 556 Mbit/s without TX stalls.
- The final FIT `cc4a048342b6c9345f77125e2fe369a209f0a69530ddd15e8e539605a98d93f3` cold-booted with XPCS0 `0x388b`. A final 10-second simultaneous receive run processed 1,677,642 packets with 26,205 budget exhaustions, only 194 hardware RX IRQs, matched release/refill publications, and zero invalid-DMA, page-map, refill-deficit, TX pending/timeout/stall or interface errors.
- A zero-budget NAPI invocation now performs TX completion reclaim only and returns without using page_pool or completing RX NAPI, matching the kernel's budget-zero contract.
- Validation artifacts and checksum manifest are archived under `out/napi-fairness-20260821/`.

### 11. Further Performance Work

- Profile checksum preparation, coherent payload copy and socket/send processing.
- Evaluate bounded doorbell batch limits only if latency requires it.
- IRQ/application CPU split is now the initramfs default: Ethernet IRQ on CPU1, init/iperf/shell on CPU0.
- Do not enable generic hardware TSO or unproven checksum modes.

## Reliability Work After P2

1. Descriptor/refill DMA ownership and coherency normalization through the DMA API: complete for every active WAN/LAN TX descriptor, RX descriptor, RX page buffer, normal refill ring and TX payload path; unused hardware rings retain vendor reserved-memory addresses.
2. TX watchdog, timeout diagnostics and bounded completion recovery: complete.
3. Recoverable RX refill starvation: complete; unknown-DMA/stale-map recovery remains fail-closed and diagnostic-only.
4. IDM IRQ routing and unused-source ownership: complete; RX filtering policy is explicitly documented from board evidence.
5. Concurrency-safe 64-bit packet, byte, error, drop and diagnostic accounting: complete.
6. ZX279051 Linux-owned cold-reset independence: complete. GPIO1 reset ownership, the vendor-derived 117-write fullmask and calibrated power sequence, explicit checked wake, reset-backed reattach and generic `phy_init_hw()` recovery are board-validated. Full platform system sleep remains outside the current `CONFIG_SUSPEND=n` build.
7. VLAN IPv4/IPv6 TCP/UDP, exact MTU/PMTU boundaries and five-minute bidirectional stress: complete; simultaneous bidirectional CPU contention and the small DSA software RX-drop count remain documented performance/accounting observations.
8. Standard NAPI weight, budget-zero compliance and mixed WAN/LAN RX queue fairness: complete.
9. ZX279051 dynamic speed-code lifecycle: complete for repeated live 1G↔100 transitions and a fresh live 1G→2.5G transition. The driver replays host SerDes/XPCS bring-up when the raw GE speed code changes even if the PHY interface remains SGMII, and still reconfigures the SoC SerDes when the interface changes to 2500BASE-X. A 10 Mbps host link remains unavailable for retest.
10. XPCS probe cleanup: complete for both SGMII and 2500BASE-X. The production link-up paths no longer dump diagnostic registers or transiently toggle VEND2/PCS bit 9. Exact final FIT `451578b65f375a13134a4e44f6e50c337f093cd678c20968418c8838ecc63ea3` passed 2.5G ping 5/5, 2.35 Gbit/s host-to-board and 1.94 Gbit/s board-to-host with zero retransmits, zero probe logs and zero driver fault counters. The temporary one-shot module requested a 2500baseT-only PHYLIB software bitmap and triggered AN restart, but later register auditing proved the current ZX279051 helper still programmed the same multi-rate line image (`7.16=0x1de1`, `7.32=0x2081`, C22.9=`0x0300`); it did not physically force 2.5G and is not a retained workaround.
11. MF ERAM link-up log cleanup: complete. The hardware write/readback and fail-closed error path remain unchanged, but successful readback is `dev_dbg` instead of an unconditional per-link `dev_info`.
12. SE/global-table probe cleanup: complete. Temporary SE SDT/stat descriptor reads and the global-L2MTU success readback were removed without changing any initialization write.
13. LAN/IDM bring-up log cleanup: complete. The first-eight-packet LAN RX descriptor dump and its counter were removed, and the static IDM TX prepare summary is now `dev_dbg`; no datapath programming changed. With WAN on en8 at 2.5G and LAN on en0 at 1G, the final FIT sustained 2.35/1.94 Gbit/s WAN and 940/941 Mbit/s LAN receiver throughput, preserved concurrent WAN ping and XPCS0 `0x388b`, and had zero combined production-cleanup logs or shared driver faults. Final combined artifacts are under `out/xpcs-sgmii-cleanup-20260821/`.
14. en8 multigig cold-start fallback: fixed with true Linux cold-reset independence. The driver now replays the complete vendor fullmask/power/calibration contract after GPIO1 reset, so the Realtek `0bda:8156` / `AppleUSBNCMData` peer selects 2.5G directly across cold boots and reset-backed reattach. No force-speed module or blind AN retry is retained. Periodic marginal-link NBase-T/high-BER policy remains deferred.
15. ZX279051 periodic NBase-T/high-BER audit: complete as an evidence gate, without a production write path. The vendor 100 ms policy, five-minute high-BER threshold, default soft-AN routine and exact trigger states were recovered. On the available 2.5G cable the high-BER gate stayed `0x0a` versus trigger `>0x45`; a reversible downshift reached 1G, restoring only the 2.5G bit remained at 1G without satisfying the vendor soft-AN trigger, and a standard AN restart returned to 2.5G with 5/5 ping. The exact final fullmask FIT also passed controlled 100M at 94.1 Mbit/s receive, controlled 1G at 941 Mbit/s host-to-board receive, repeated restoration to 2.5G, and ten consecutive reset-backed eth0 down/up cycles while LAN remained active; all final fault counters were zero and release/refill counts matched. Raw vendor soft-AN and automatic high-BER writes remain deferred until their natural trigger can be reproduced. Evidence is archived under `out/zx279051-runtime-adaptation-audit-20260821/`.
16. RTL8372N production-path cleanup: complete. Gate-era module parameters are removed, the validated DSA sequence is unconditional, probe/remove diagnostic sweeps are absent, and success logs describe production state. Exact FIT cold boot and same-boot reload passed at WAN 2.5G/LAN 1G with no parameter directory, zero checked diagnostic logs, XPCS0 `0x388b`, clean pings/throughput and zero driver fault counters. Review and artifacts are under `out/rtl8372n-production-cleanup-20260821/`.
17. NPPT global reset/reinitialization lifecycle: complete for quiescent last-owner teardown. Ten alternating LAN-first/WAN-first cycles restored global-done `0x1fd`, reset word `0xffffffff`, XPCS0/1 `0x388b/0x280b`, WAN 2.5G and LAN 1G; final throughput was 2.35 Gbit/s WAN RX and 940/941 Mbit/s LAN RX/TX with zero driver/recovery faults and matched release/refill counts. In-place reset under active DMA remains explicitly unvalidated and is not wired to `ndo_tx_timeout`. Evidence is under `out/nppt-global-reset-validation-20260821/`.
18. Thirty-minute dual-path soak: complete. Four simultaneous WAN/LAN bidirectional streams each produced 1800 continuous one-second intervals and exited successfully. All 31 board samples retained global-done `0x1fd`, reset word `0xffffffff`, XPCS0/1 `0x388b/0x280b`, zero driver/recovery faults and equal release/refill counts while processing 114,490,293 RX completions. Aggregate contention-limited throughput was about 1.41 Gbit/s; WAN/LAN reverse retransmissions were 272/21,105 and are retained as performance evidence. Post-soak LAN-first and WAN-first reprepare, pings and isolated throughput passed. Evidence is under `out/nppt-dual-path-soak-20260821/`.

## Deferred

- Vendor-compatible 1996-byte CPU-WAN frame support: deferred. The dual-bank implementation requires further recovery of vendor refill/queue semantics; both 32- and 1024-entry jumbo-bank experiments stopped the RX data plane after about 2K packets. The stable source and board remain at L2 1984 / MTU 1970.
- True 9K jumbo: closed as a project target. The vendor runtime retains a roughly 1996-byte CPU-WAN limit, so software MTU 9000 is not evidence of a supported 9K datapath.
- RTL8372N remaining switch operations: direct two-endpoint software bridge forwarding is deferred until a second independent LAN endpoint is available; tagged/trunk VLAN support still requires an evidenced hardware contract. Private transport VLANs 59..62 remain internal.
- PTP and advanced offloads: after WAN reliability and upstream cleanup.

## Evidence Rules

- Keep `vendor-reference` immutable.
- Keep one source variable under hardware validation at a time.
- Archive each FIT, SHA-256 and board result under `out/`.
- Do not promote a register meaning or offload capability without source and live evidence.
