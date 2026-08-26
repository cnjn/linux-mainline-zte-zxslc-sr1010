# ZX279133 Minimum Forwarding Analysis

## Objective

The first mainline path needs only CPU/IDM traffic to and from XMAC1 through
NPPT outport 6. This document separates the vendor's mandatory hardware
foundation from PON, QoS, offload, Wi-Fi, and switch policy. No forwarding
registers are implemented until the remaining dependencies are reduced to
individually testable operations.

## Vendor Initialization Root

`np.ko` module entry initializes software databases and MMIO mappings, then
calls `tm_initial()`. The latter performs this fail-fast hardware sequence:

1. Disable SIPC RX and preserve each SMAC configuration.
2. `np_tm_init()`:
   `dma_init -> tm_bmu_init -> tm_qmg_init -> sopc_init -> red_init ->
   tm_ssch_initial -> tm_wsch_initial -> tm_woe_sch_initial ->
   tm_usch_initial -> tm_uopc_initial`.
3. Initialize SE parser/SMMU/tables/hash/statistics.
4. Initialize PPU microcode and MTU handling.
5. Initialize NPPU blocks: SPA, ISU, ODMA, SMCT, and protocol-channel controls.
6. Initialize PPU, SDET, and SIPC, then restore SMAC configuration.
7. Mark all modules ready. A later `zte_api_np_init_done()` calls
   `mf_port_ppuinit_set()` for the selected port.

The module therefore supplies a complete product forwarding plane, not a
minimal Ethernet MAC helper. Copying `tm_initial()` would import unrelated
policy and large reserved-memory dependencies.

## Proven Fixed Foundation

The following operations have exact arguments and are plausible prerequisites
for any CPU-to-XMAC packet path:

| Block | Vendor operation |
| --- | --- |
| DMA | AXI mode 0; RID groups `(12,11,10)` and `(15,14,13)`; WID `(14,12,11)`; read-SE command FIFO gap 8 |
| QMG | descriptor input/output limits, watermarks, descriptor RAM initialization, and init-done polling |
| SOPC | disable hardware CRC padding for MACs 0 through 6; CPU133 sets combined up/down FIFO threshold 2056 |
| SDET | upstream/downstream minimum frame 12 and maximum frame 2048 |
| SIPC | RX-SOP FIFO gap 4162, SPA 13-clock enable, CPU133 scheduler mode 0, then RX enable |
| Route | NPPT `0x19c.bit0=1`, PON route word 0, NPPT `0x2438.bit2=0` for XMAC1 |

The current mainline MAC lifecycle already owns and validates the route row.

## Blocks Not Yet Reducible

- **BMU** obtains a separate reserved DDR region, constructs BPPE pools, flushes
  caches, and programs low/high thresholds. The minimum pool shape is not yet
  isolated from vendor buffer policy.
- **QMG and RED** initialize hundreds of queue thresholds and descriptor RAM.
  CPU queue 1 and RX queue 0 cannot be enabled safely until their shared-global
  limits and RAM-init dependencies are identified.
- **Schedulers** are initialized globally. SSCH and USCH are likely on the
  direct path; WSCH/WOE/UOPC may be product-wide dependencies but are not yet
  proven necessary for a non-offloaded P2P packet.
- **SE/PPU/NPPU** provide parsing, classification, source/destination processing,
  ODMA, and SMCT. CPU TX carries outport 6 directly in descriptor byte `0x0a`,
  so a classifier default rule is not the missing TX destination selector. The
  downstream descriptor/buffer path still depends on these blocks.
- Exported `zte_api_pp_global_init`, `zte_api_pp_buff_init`, and switch APIs add
  protocol traps, MAC learning, VLAN, TPID, QoS, and hundreds of RED queue
  entries. These are policy and are excluded from the initial driver unless a
  specific packet transition demonstrates a dependency.

## Current Hardware Handoff

U-Boot runs its own `np init` before TFTP and leaves a functioning CPU/XMAC1
forwarding plane. This handoff enabled the captured descriptor transition in
`IDM_DESCRIPTOR.md`, but its register layout differs from vendor Linux in ring
base ordering and RX-depth encoding. It may be used for read-only diagnosis and
first controlled DMA experiments, not as the final upstream initialization.

`greg_init_done_check()` provides one coarse NPPT readiness check: NPPT `0x80 &
0x1fd` must equal `0x1fd`. This confirms global blocks report ready but does not
prove queue routing or buffer ownership.

## Controlled TX Transition

FIT SHA256 `08a179e925b0d4ecb50e97d2be199813bc0a0bc0b53ef5515d90c9630d8e5e6c`
allocated a 128 KiB coherent four-queue TX descriptor region, replaced only IDM
TX base `0x004`, and submitted one packet at a time through vendor Linux queue 1.
The descriptor used selector `0x0f`, outport 6, a DMA-mapped payload, `dma_wmb()`,
and doorbell `0x20000` at IDM `0x0a0`.

Queue-1 completion at IDM `0x0ac` advanced from zero to three. Mainline reclaimed
all three mappings and reported 3 packets / 180 bytes with no drop. This proves
that the dedicated CCI lifecycle makes coherent descriptors and streaming
payload mappings visible to IDM.

The peer captured no frame and XMAC1 send counters remained zero. A second FIT,
SHA256 `08704cfe09cb01fb910e91c5937d664eb18f7469369507164fc6c78b0b7c3b31`,
repeated the experiment with U-Boot's queue 0. Its completion advanced from
`0x0bc7` to `0x0bd3`, and the descriptor at the expected producer slot contained
the mapped address, `0x0f000078` length/selector, and outport word `0x00460000`.
XMAC1 send counters still stayed zero. These experiments prove descriptor DMA
visibility and outport-field publication, but they did not yet reproduce the
complete vendor Linux descriptor contract.

The controlled code returns to vendor Linux queue 1 after this isolation test.
It remains single-packet and TX-only; it does not claim a functioning netdev
forwarding path.

Final queue-1 FIT SHA256
`bcef9d60bb9cc782b82e7e959342e71f96e9d72c5cd158979968ca25c9c9b214`
repeated completion `0 -> 3`, 3 packets / 180 bytes, and zero drops. Stop
restored both CCI words to `0x00000020` and returned `pon_idm_aclk` and
`pon_serdes_pclk` to enable count zero.

## `plat_132` Contract Re-Audit

The vendor Linux call order is `cpu_net_pon_set_desc()` followed by
`idm_cpu_tx()`. The first helper writes descriptor `+0x18 = 0x08000000`, clears
`+0x10/+0x14`, and selects `lan_up_port=6`; the second writes address, length,
selector, and outport. Earlier controlled descriptors incorrectly cleared
`+0x18`. U-Boot's observed bit 23 belongs to its different descriptor contract
and is not present in recovered vendor Linux `idm_cpu_tx()`.

The mainline descriptor now includes the recovered `+0x18` word and remains on
vendor Linux queue 1. FIT SHA256
`c037810742bf103389031ad54e9fea92b94d411c8f026d04d77e5e5cfc859b82`
proved that this correction alone was insufficient against the U-Boot handoff.

Recovered `greg_init()` uses vendor Linux exported constants, not U-Boot values:
NPPT `+0x68=0x27800940`, `+0x6c=+0x20078=0x781`, SMAC0-3 runt mask bit 18,
SMAC6 runt mask bit 16, and XMAC0-1 mask `0x410`. An earlier U-Boot-derived
`0x28000800/0x5ee` experiment was corrected and is not part of the retained
implementation.

This closes the known `plat_132` platform and direct-TX descriptor omissions.
The remaining TX prerequisite is initialized later by `np.ko::tm_initial()`;
U-Boot's forwarding state is not a valid substitute for that Linux contract.

## First `np_tm_init` Stage

The first `np_tm_init()` callee, `dma_init()`, is fully fixed and has no buffer
policy. Register-table recovery maps its operations to:

| NPPT offset | Vendor Linux operation | Programmed value |
| --- | --- | --- |
| `0x38000.bit21` | AXI mode 0 | clear |
| `0x38080` | RID `(12, 11, 10)` | `0x000c0b0a` |
| `0x38084` | RID `(15, 14, 13)` | `0x000f0e0d` |
| `0x38088` | WID `(14, 12, 11)` | `0x000e0c0b` |
| `0x38398[14:10]` | read-SE command FIFO gap 8 | `8` |

FIT SHA256 `6dcb7317660301659980ebba8cea72e0b93d76193384ed911cda30969969dc77`
read back all five fields exactly. Queue-1 completion again advanced `0 -> 3`
with 3 packets and zero drops; no end-to-end reply was received. Stop restored
CCI to `0x20` and returned checked clocks to zero.

The next callee is `tm_bmu_init()`. Vendor Linux boot evidence fixes the BMU
region at `0x9f100000` with size `0x0f00000`; the initializer reports an actual
requirement of `0x0ecc000`. Mainline must model this reserved region before any
BMU pool or threshold register is written.

## BMU, QMG, and SOPC Stages

The SR1010 DTS now reserves `0x9f100000..0x9fffffff` as a `no-map` BMU region and
passes it through the standard `memory-region` property. FIT SHA256
`6bad3ec8c0b704136050838ee0f6c33535e89dbbd3acb241b1a0ad6750a36f57`
confirmed a contiguous 15 MiB reservation and successful NPPT probe without BMU
register writes.

Vendor ELF constants close the `tm_bmu_init()` layout exactly:

| Object | Count/size | Physical range start |
| --- | --- | --- |
| BPPE tables | `0x20000` bytes | `0x9f100000` |
| Normal buffers | `0x1800 * 0x840` | `0x9f120000` |
| Jumbo buffers | `0x20 * 0x2600` | `0x9fd80000` |
| Descriptor storage | `0x200000` bytes | `0x9fdcc000` |

The sum is exactly `0xecc000`. FIT SHA256
`29b3a914234328c4c3b5fc074d3adcc3215a7ac020cd4d455aff3d26ad285c56`
validated both BE16 BPPE index tables, every base/count/size register, BPPI
thresholds, and BMU enable. Hardware advanced the normal/jumbo index state after
enable, proving the block consumed initialization. XMAC1 TX remained zero.

The CPU133 `tm_qmg_init()` stage then programmed descriptor limits, watermarks,
VR gaps, downstream/WOE enables, and requested all five RAM initializers. FIT
SHA256 `a5875ccd719b42716f196fa1e1d98b26a35e8b176da25efe43ed304ea54ded0c`
read back QMG RAM done `0x1f` and all fixed values; XMAC1 TX remained zero.

Finally, `tm_sopc_initial()` clears MAC0-6 CRC/padding policy bits and programs
the shared FIFO threshold to decimal 2056 (`0x808`). FIT SHA256
`1d103c3c2a18c7b99a27d5f09ca253023c702eaa538b9ed13209f31fd581954b`
read back `0x0/0x808`, retained carrier, and completed three queue-1 descriptors
with zero drops. The peer and XMAC1 still observed no frame. Stop restored CCI
to `0x20` and returned checked clocks to zero.

The next CPU133 `red_init()` prefix programs fixed sharing modes and global
limits before any per-queue policy. The recovered fixed values are:

| RED field | NPPT offset | CPU133 value |
| --- | --- | --- |
| enable, color, and sharing modes | `0x20004[7:0]` | `0x7e` |
| total input share maximum | `0x20040[13:0]` | `0x1400` |
| CPU133 fixed configuration | `0x2024c` | `0x000a0001` |
| IDM / WOE input share maxima | `0x20344/0x20348[14:0]` | `0x500/0x500` |
| upstream / downstream input maxima | `0x20060/0x20064[13:0]` | `0x1200/0x400` |
| upstream / downstream output maxima | `0x20074/0x20070[15:0]` | `0x1000/0x3000` |
| all / WOE output share maxima | `0x2006c[15:0]`, `0x2034c[14:0]` | `0x4000/0x3000` |
| upstream / downstream descriptor watermarks | `0x20084` | `0x60002000` |
| IDM descriptor watermark | `0x20088[15:0]` | `0x500` |

`red_np1_config()` programs six NP1 queue thresholds and
`tm_red_buffer_initial()` programs 401 input/output queues. They are deliberately
excluded until their product policy and the selected queue path are reduced.
FIT SHA256
`95c279088513931c9a770991a61254555f7ccb0b7f30c3709863f83c40433204`
read back every fixed field while retaining shared-register bits. Carrier was 1
and queue-1 completion advanced `0 -> 3` with zero drops, but XMAC1 and the peer
still saw no frame. Stop restored both CCI words to `0x20` and all seven checked
clock enable counts to zero.

`tm_ssch_initial()` is entirely fixed on CPU133: it enables sharp and DWRR,
enables queue aging in mode 1, sets loop/spend bytes to `8/20`, sets sharp fill
time to `1024`, and writes aging time `250000000`. These collapse to four NPPT
registers:

| SSCH register | Offset | Value or field value |
| --- | --- | --- |
| scheduler configuration | `0x2c000` | bits 13, 12, 1, and 0 set (`0x3003`) |
| loop/spend bytes | `0x2c024` | `0x0814` in masks `0xff00/0x3f` |
| sharp fill time | `0x2c028[17:0]` | `0x400` |
| aging time | `0x2c004` | `0x0ee6b280` |

FIT SHA256
`4169eb5af0bb87fc69cabb722415fae10481ecea26d2b960c418a0ef38665465`
read back all four values exactly with carrier 1. Queue-1 completion again
advanced `0 -> 3` with zero drops while XMAC1 and peer capture remained zero.
Stop restored CCI `0x20/0x20` and all seven checked clock counts to zero.

The next three scheduler stages are also fixed on CPU133:

| Stage | Offset | Value |
| --- | --- | --- |
| WSCH sharp enable | `0x2c400.bit1` | `1` |
| WSCH fill time | `0x2c444[17:0]` | `0x400` |
| WOE scheduler sharp enable | `0x2c800.bit0` | `1` |
| WOE scheduler fill time | `0x2c828[17:0]` | `0x400` |
| USCH enables and aging mode | `0x28000[6:0]` | `0x7f` |
| USCH aging time | `0x28004` | `0x0ee6b280` |

FIT SHA256 values `6ecdb5c5356e1feb820d792ac345f7e075e47f53df1ba3aad4c746f483426410`,
`88efc5aeb3c8105e8e25f7bc62c6be3bc3913111d84707d36cc9d8425a390937`,
and `91d6241489fd7c6e5f7471d253b31d5eaaa8f3908efbdc229ffaaea2bb97fb57`
independently validated WSCH, WOE scheduler, and USCH. Every image retained
carrier 1 and queue-1 completion `0 -> 3` with zero drops, while XMAC1 and peer
capture remained zero. Each stop restored CCI `0x20/0x20` and all seven checked
clock counts to zero.

Vendor runtime logs prove P2P `g_pon_work_mode = 0x10`. Therefore the next
`tm_uopc_initial()` branch uses 17 TX FIFOs with depths `8/10/9`, selector groups
`0/1/2`, sequential bases ending at 160, pre-afull gaps `17`, burst counts `6`,
and info-afull gap `10`; FIFOs 17 through 39 are disabled. Its register at
`0x30000` is not ordinary state: bit 3 is a self-clearing command used for both
tcont mode and tcont initialization. A safe stop policy must be established
before this stage is written.

The P2P UOPC implementation saves 14 ordinary registers but never saves or
restores command register `0x30000`. It waits for bit 3 to clear before and after
both tcont-mode and tcont-init requests. The resulting image enables FIFOs 0-16,
disables 17-39, and preserves fields that vendor Linux does not touch. FIT
SHA256 `471af28c0d9316ec48992ec1346dde1ed469d165dc23cb31890d49ba5da686d0`
read back the entire image exactly. Queue-1 completion still advanced `0 -> 3`,
while XMAC1 and peer capture remained zero; UOPC is therefore initialized but
is not the final missing XMAC stage.

The next `tm_initial()` callee is `se_init()`. Vendor runtime assigns its hash
DDR to `0x9e100000/0x01000000`; the second DDR-table region starts at
`0x9f100000` but has size zero. The board DT now owns the hash range as
`se-hash`, adjacent to and non-overlapping with BMU, and NPPT probe validates
its size. FIT SHA256
`d11c4d5633c4d0a86bfe64a5065183a6485ab14f4641dd0db845f0c511edd87b`
confirmed the reservation, probe, UOPC image, carrier, and clean stop.

SE register-table IDs are not all in the NPPT MMIO domain. Assembly at both
`se_parser_init()` and `se_smmu0_init()` explicitly sets module flag 1, so
`np_onu_reg_read()` selects PPS. The real addresses are PPS `0x40008`,
`0x48084`, and `0x400c0`; physical addresses are `0x18040008`, `0x18048084`,
and `0x180400c0`. They read parser done `0x1f`, SMMU0 done `1`, and debug mode
`1`. The earlier `0x80000001` result came from the unrelated NPPT domain and is
not evidence of a missing RAM request.

Mainline now reproduces the preceding vendor reset path: isolate SDET through
`0x2c0004.bit4`, pulse bit 31 low, initialize SIPC, poll global ready
`0x80 & 0x1fd`, then restore SDET. SIPC ends at `0x4000[2:0]=3` and
`0x4010=0x1042`. PPS is modeled as the second named NPPT MMIO resource, and the
SE frontend polls/configures only that mapping. FIT SHA256
`912b81dfc3fe34258fce74969fb5cb162613157ea8a331d6a986837a57d32754`
validated this sequence, PPS readiness, queue-1 completion `0 -> 3`, and clean
stop. XMAC1 and peer capture remained zero.

## Remaining tm_initial() Order

The full post-`se_init()` order is now recovered: `np_ppu_init()` downloads the
PPU microcode from `/etc/mcode_133/mcode_intel.bin` and programs L2/L3 MTU;
`np_nppu_init()` runs `spa_init`, `isu_init`, `odma_init`, `smct_init`, the IPv6
CRC mode, and six global OAM enables; `ppu_init()` programs group policy; then
`np_sdet_init()` sets four frame lengths and `sipc_init()` re-enables the
input stage.

## SMCT and SDET Contracts

The CPU133 `smct_init()` stage is fixed values with no queue loops:

| Register | Value | Meaning |
| --- | --- | --- |
| `0xc000` | `0x1f00` | selection configuration |
| `0xc004` | `0x1fff` | selection configuration |
| `0xc008` | `0x9` | CRC enable plus control bit |
| `0xc028..0xc030` | `0x1b001b00` | cos 0-5 threshold pairs |
| `0xc034` | `0x1d001fff` | cos 6/7 threshold pair |
| `0xc03c..0xc048` | `0x1c001c00` | input port 0-7 threshold pairs |
| `0xc04c` | `0x1c001d00` | input port 8/9 pair |
| `0xc050` | `0x1d001c00` | input port 10/11 pair |
| `0xc054` | `0x1fff1fff` | input port 12/13 pair |
| `0xc058` | `0x1c001fff` | input port 14/15 pair |
| `0xc038[13:0]` | `0x1c00` | multicast threshold |
| `0xc05c` | `0x1a00` | NP1 threshold |
| `0xc060` | `0x1a001a00` | IDM and SSCH thresholds |

`np_sdet_init()` writes two registers: `0x2040 = (2048 << 14) | 12` and
`0x2044 = (2048 << 16) | 12`, preserving vendor-untouched high bits. Both
stages are implemented with save/restore; vendor-untouched bit ranges in the
pair registers are preserved. FIT SHA256
`db31530b9f8ebe312ff7ffbeda5ffac26a4d5b7288406f28be3f180c619bea09`
builds cleanly. Board validation is blocked because the host rebooted and no
USB serial adapter is currently enumerated.

## Next Reduction

1. Validate SMCT/SDET on hardware as soon as the serial adapter is reconnected.
2. Recover the remaining fixed `np_nppu_init()` stages: ISU request plus poll,
   ODMA enable, SPA fixed tables, IPv6 CRC mode, and the six global OAM
   enables.
3. Keep PPU microcode, SPA policy tables, SE hash tables, and PPU group policy
   excluded until a specific packet transition proves a dependency.
3. Trace RX source port 6 through SDET/SMCT/ODMA after the TX foundation works.

Until those traces are complete, Phase 6.1 remains active. Controlled TX DMA is
enabled only for one in-flight queue-1 descriptor; RX, NAPI, and IRQs remain
disabled.

## Mainline Flow Offload Frontend

The factory acceleration frontend is split across two modules. `switch.ko`
translates the vendor FFE flow object into a hardware-fast session, while
`np.ko` exports `zte_api_fast_l3_session_add()` / `del()` and emits the NPPT
fast-table entry. Mainline does not need the proprietary FFE layer: the
equivalent ingress is `TC_SETUP_FT`/flower, which already supplies the exact
match, rewrite, and redirect information.

`drivers/net/ethernet/zte/zx279133-offload.c` now implements that direct path
for exact-match IPv4 TCP/UDP forwarding between the LAN conduit and WAN:

- the original five-tuple becomes the 16-byte SDT 43 multi-hash key;
- Ethernet, route, source/destination NAT, and port-NAT rewrites become the
  32-byte fast response;
- source NAT updates WANID 0's source-IPv4 word while the translated address
  is in use;
- the recovered SDT 43 configuration extracts that key into 256-bit table ID 1,
  and entries use a 32-byte ZCAM row with footer `0xc1`;
- placement uses the four vendor ZCAM blocks, five cells per block, and the
  alternating CRC16 polynomials `0x1021`/`0x8005`, providing 20 candidate
  locations per key and 5,120 physical slots;
- add and delete use the SE algorithm indirect window at PPS `0x50000`.

SDT 43 selects hash ID 0, exactly as the factory `0xb8` descriptor does, so
fast IPv4 entries do not use the 16 MiB DDR window reserved for hash ID 1. The
effective table limit is instead 4,096 independent IKEY/age entries, or 2,048
bidirectional connections. The first 1,024 hardware directions additionally
own exact SDT29 packet/byte counters; later directions leave the response's
statistics-enable bit clear and use the independent SE age bit for truthful
`lastused` refresh.

Capacity acceptance on 2026-08-26 installed 2,048 bidirectional UDP NAT
connections: all 4,096 flower directions reported `in_hw_count 1`, and the
2,049th connection failed with `-ENOSPC` without leaving a partial rule. All
4,096 entries then deleted cleanly. A focused collision set installed 32
bidirectional connections whose LAN keys share ZCAM block 0/cell 0/address
`0x42` and whose reverse keys share block 0/cell 0/address `0x0e`; all 64
directions reached hardware through the remaining cell/block candidates.

The WAN and LAN conduit netdevs advertise `NETIF_F_HW_TC`. Each bound flow
block retains its actual netdev, so ordinary clsact rules use the bind device
as ingress while nft flowtable rules may override it with the META ifindex.
The diagnostic configuration includes nftables flowtable, flower/ingress, and
the pedit, mirred, and checksum actions required to exercise the frontend.

Board acceptance on 2026-08-23 used FIT SHA256
`6611b4cee0fe7bddeee654a2195774e5710f4f689e3a984fbe51a2c229c506bb`:

- the image RAM-booted, PPU microcode reported `0x10001.0x1`, WAN reached
  carrier, and the `192.168.1.100` peer replied;
- a LAN-to-WAN exact TCP route reported `skip_sw`, `in_hw`, and
  `in_hw_count 1`;
- a LAN-to-WAN SNAT rule rewriting the IPv4 source and TCP source port, with
  checksum update, reported the same hardware status;
- a WAN-to-LAN exact TCP route also reported `in_hw_count 1`;
- every rule deleted cleanly, both clsact blocks unbound, and dmesg contained
  no BUG, Oops, WARNING, or WANID-restore failure.

This proves the flower-to-SDT14 add/delete path and both forwarding directions
on silicon, including WANID-backed source NAT. Matching-packet counters and
throughput were separate acceptance items at this milestone and are closed by
the later performance and hardware-statistics sections. The final clean
build is `/Volumes/code/zx279133/out/sr1010-zxdbg.itb`, 5,193,488 bytes,
SHA256 `b348949946d04fcb9d7f1388bd1e88ad97d430562e02a0229c8d438953d6b726`;
the only source change after the board-accepted image was whitespace alignment.

## Untagged VLAN Action and Packet Acceptance

Board acceptance on 2026-08-25 identified the final packet-delivery dependency.
The vendor runtime initializes SE ERAM SDT 0 with the VLAN action template
`ffc00000 0000ffff ffffffff 00003003`, while mainline left the table at zero.
The accelerated IPv4 path indexes entry 0 for these untagged packets. A live
A-B test was deterministic: clearing SDT 0 entry 0 produced `0/5` UDP echo
replies, and restoring the factory entry produced `5/5`.

`zx279133_vlan_runtime_prepare()` now writes only the required VID 0 entry.
The write follows `zx279133_route_set(..., true)`: writing it earlier in
`zx279133_np_prepare()` left the ERAM contents readable but did not activate
packet delivery after the route gate changed state. The first FIT containing
this fix was 5,178,688 bytes, SHA256
`58ea78c319c4df5242fd33b224dbeac4d282829bdc55914f592d4631ec59566a`.

That FIT was RAM-booted without any live table patch. After the initial
link/neighbor warm-up, the exact UDP SNAT/DNAT flow delivered `10/10` replies.
Four paced UDP flower flows all reported `skip_sw`, `in_hw`, and
`in_hw_count 1`. Over ten seconds they forwarded 1.900 Gbit/s of UDP payload;
the Mac en8 hardware counter measured 1.935 Gbit/s including Ethernet framing,
with 25 packets difference out of 1,613,448 offered packets (0.00155%). An
overload run saturated near 2.015 Gbit/s at en8. This closes functional packet
delivery and established the initial 1.9 Gbit/s lossless hardware-NAT point.
The overload result was then used to localize the remaining performance limit.

## Traffic Manager Profiles and XMAC Flow Control

The factory RED queue image differs by traffic class. Mainline now programs
the recovered factory buffer template and the forwarding-queue profiles:

- queues 320 through 359 use output maximum `0xe00`, guard `0x40`, and maximum
  `0x200`;
- queues 360 through 375 use output maximum `0xe00`, guard `0x80`, and maximum
  `0x400`;
- queue 360 reads back RAM 0 `0x00e00020`, RAM 2 `0x01000080`, and RAM 4
  `ff803fff 0100ff80 00100200 00000020`.

Matching the factory RED image removed a reconstruction error but did not move
the overload ceiling. Direct A-B tests also ruled out the differing global RED
high fields, SSCH spend-byte value, and an SSCH configuration bit. The latter
stopped XMAC1 output when enabled in the mainline initialization order and was
not retained.

The actual limit was XMAC1 pause handling. Mainline advertised symmetric and
asymmetric pause through phylink, producing `FLOW_CTRL=0xffff0002` and
`RX_FLOW=1`; the same 2.3 Gbit/s payload offer then reached about 2.015 Gbit/s
and lost roughly 13.7% of packets. The factory runtime instead leaves both
pause-enable bits clear (`FLOW_CTRL=0xffff0000`, `RX_FLOW=0`). The decisive
single-bit tests were:

- both pause-enable bits clear: 2.365416 Gbit/s received versus 2.365624
  Gbit/s sent, 0.00865% packet difference;
- only `RX_FLOW` bit 0 set: 2.049945 Gbit/s received, 13.34446% packet
  difference;
- `FLOW_CTRL` TFE set with `RX_FLOW` clear: 2.365483 Gbit/s received, 0.00579%
  packet difference.

The WAN MAC therefore no longer advertises pause capabilities. This keeps
phylink negotiation, ethtool reporting, and the XMAC register state consistent
with the factory datapath instead of silently overriding an advertised mode.

Final cold-boot acceptance used
`/Volumes/code/zx279133/out/sr1010-zxdbg.itb`, 5,179,112 bytes, SHA256
`6ae6893d7ca60bbec35ddef8fb262e400ff642cbf9c536e222659d22dbbce484`.
The canonical UDP SNAT/DNAT flow returned `10/10`, and both directions reported
`skip_sw`, `in_hw`, and `in_hw_count 1`. Four paced UDP flows offered 2.3
Gbit/s of payload for ten seconds. Windows transmitted 1,953,125 packets at
2.365624 Gbit/s including link framing; Mac en8 received 1,952,835 packets at
2.365274 Gbit/s, a 0.01485% packet difference. The performance flows were then
removed, the canonical flow still returned `10/10`, and dmesg contained no
BUG, Oops, WARNING, WANID failure, or SMMU timeout.

This performance image did not yet report hardware fast-entry statistics. The
subsequent per-flow packet, byte, and last-used implementation and its exact
counter acceptance are recorded separately in
[FAST_STATS_RESEARCH.md](FAST_STATS_RESEARCH.md).

## UDP NAT Line-Rate Acceptance

The same cold-booted FIT was accepted again on 2026-08-26 with one exact UDP
five-tuple and 1472-byte payloads. Four sender threads shared one socket (or
four `SO_REUSEPORT` sockets with the same local port in the reverse direction),
so the packets on the wire still formed one flow.

- LAN to WAN: Windows transmitted 6,093,347 packets at 2.459645 Gbit/s and
  Mac en8 received 6,092,929 packets at 2.459479 Gbit/s over 30 seconds, a
  0.00686% packet difference.
- WAN to LAN: Mac en8 transmitted 2.458635 Gbit/s and Windows received
  2.458716 Gbit/s over 30 seconds. The generator submitted 6,093,128 packets
  and Windows received 6,089,966, a 0.05189% packet difference.
- Three additional hot LAN-to-WAN repetitions received 2.459383, 2.459958,
  and 2.460054 Gbit/s with no sender errors.

These rates are the expected 2.5GbE limit after Ethernet framing, preamble,
and inter-packet gap overhead. Temporary performance rules were removed after
each run; the canonical UDP flow still returned `10/10`, both canonical rules
remained in hardware, and the kernel log remained clean. Reproducible rule and
sender sources live in `port/mainline/nat-acceptance/`.

## nftables Flowtable Integration

The diagnostic image now contains a statically linked nftables 1.1.6 binary,
enables IPv4 forwarding, and exposes conntrack and flowtable procfs state.
`nft-flowtable.nft` uses an ordinary inet flowtable over `lan1` and `eth0`, an
accepting forward chain with `flow add`, and an IPv4 SNAT postrouting rule. No
TC flower rule is installed for this test.

On the first RAM boot of the image, a normal UDP/5000 exchange returned
`20/20`; `/proc/net/nf_conntrack` marked the translated connection
`[HW_OFFLOAD]`. A separate UDP/5202 connection then sustained 2.459986 Gbit/s
at the Windows transmitter and 2.459971 Gbit/s at Mac en8 for 30 seconds, with
a 0.00056% packet difference.

A 70-second run with periodic reverse traffic remained `[HW_OFFLOAD]` at the
5, 30, and 60 second checkpoints. Windows transmitted 14,217,379 packets at
2.459995 Gbit/s and Mac received 14,216,465 packets at 2.459836 Gbit/s, a
0.00643% packet difference. Immediately flushing the nftables ruleset changed
the conntrack state from `[HW_OFFLOAD]` to `[ASSURED]` while preserving the
software connection, proving the standard hardware-delete lifecycle.

The source state was also accepted without U-Boot network initialization:
Linux ran `reboot -f` and the next U-Boot shell ran only `bootm`. Two
consecutive direct boots reached 2.5 Gbit/s. The ZX279051 driver now retries
autonegotiation once when 2.5G is advertised locally but the first result
falls back below 2.5G; a forced 1G negotiation exercised the fallback and
recovered to 2.5G. During the direct-boot performance acceptance, the
automatic nftables UDP flow reached `[HW_OFFLOAD]` with no manual TC rule and
Mac en8 received 6,092,547 of 6,093,128 packets at 2.459733 Gbit/s over 30
seconds, a 0.00954% packet difference.

The final FIT, SHA256
`f17db5af220c4cacacf5c3f1f067c3e9f01c427587ee42ee33f8fe4936533d5d`, repeated
the direct `bootm` acceptance at 2.5 Gbit/s; its embedded nftables 1.1.6 also
created `[HW_OFFLOAD]` for an ordinary UDP connection. The kernel log contained
no BUG, Oops, WARNING, WANID, or SMMU failure.

`CONFIG_NFT_CT=y` and a `ct state established` guard now defer `flow add`
until conntrack has observed a reply. This prevents one-way UDP from entering
hardware as `[UNREPLIED]` and expiring on the short unreplied timeout.

An unreplied UDP connection still uses the short conntrack timeout and may
leave and later re-enter the hardware path. This is normal for a one-way UDP
probe and is not representative of an established NAT exchange. Real hardware
packet, byte, and last-used reporting was accepted in the later statistics
milestone documented in [FAST_STATS_RESEARCH.md](FAST_STATS_RESEARCH.md).

## Single-flow TCP NAT Acceptance

The same nftables ruleset was accepted with one established TCP connection in
each direction. LAN-to-WAN transferred 4,445,372,416 payload bytes in
15.002875 seconds (2.370411 Gbit/s); WAN-to-LAN transferred 4,443,627,792
payload bytes in 15.004102 seconds (2.369287 Gbit/s). Both connections showed
`[HW_OFFLOAD]` while active and disappeared from conntrack after close. The
SPA TCP-control mask remains at the vendor value `7`, so FIN and RST packets
leave the fast path for normal conntrack teardown. `TcpStream.cs` preserves
the single-connection Windows harness.

## Bidirectional and Packet-Rate Acceptance

The nftables flowtable submits an original and a reply `FLOW_CLS_REPLACE` for
an established connection. A temporary diagnostic build confirmed that both
UDP rules were accepted and programmed; the duplicate callback from the other
bound flowtable device returned `-EEXIST` only after the same cookie was already
installed. The diagnostic logging was then removed completely.

WAN-to-LAN automatic NAT was accepted independently near line rate. macOS sent
2,031,040 packets in 10.099449 seconds and the Windows NIC received 1,968,968
packets / 2,980,990,123 bytes, or 2.361309 Gbit/s at the NIC counter. The
3.05617% gap between successful userspace sends and NIC receives occurred
between the sender and receiving NIC at saturation; the received NIC-counter
rate is close to the 2.5G link ceiling after framing.

Both hardware directions also ran simultaneously. At a 2.0 Gbit/s target per
direction, the LAN-to-WAN sender produced 1,898,940 packets and macOS received
1,897,670 (0.06688% difference); macOS produced 1,651,252 packets and Windows
received 1,614,693 (2.21402% difference). At maximum targets, LAN-to-WAN still
delivered 2,325,705 of 2,335,700 packets while WAN-to-LAN reached 1,687,835 of
2,031,040. Since WAN-to-LAN alone reaches 2.361309 Gbit/s and `en8` is the
host's USB 2.5G adapter, the simultaneous maximum is recorded as a testbed I/O
ceiling, not an NPPT hardware ceiling. A second independent 2.5G host interface
is needed to claim two-direction line rate. Router aggregate CPU utilization
during the maximum-target run was 0.15432% (4 busy ticks out of 2,592).

The minimum-frame test used an 18-byte UDP payload. Windows transmitted
3,202,475 packets and macOS received 3,202,467 in 10.075111 seconds: 317,859
packets/s with an eight-packet (0.00025%) difference. Router aggregate CPU was
0.33529% (8 busy ticks out of 2,386). This establishes a lossless tested floor,
not the NPPT maximum packet rate; the Windows C# sender was the limiting
generator. `UdpPaced.cs` and `udp-pacer.c` now report PPS directly, and the
macOS tool accepts an optional payload length for repeatable reverse-direction
small-packet tests.

## Multi-Flow and Lifecycle Acceptance

The synthetic `FLOW_CLS_STATS` implementation based on flow insertion time was
removed before real statistics were available. The later accepted
implementation assigns an independent SDT29 counter pair to each hardware
direction and refreshes `lastused` only when those hardware packet or byte
counters advance. The age index in the ZCAM response remains allocated as part
of the validated response format; the driver does not consume the SE age
read-clear bitmap.

Four established UDP connections on dedicated ports ran concurrently at
10 Mbit/s each. Once startup completed, all four conntrack entries showed
`[HW_OFFLOAD]` in every one-second sample from test second 2 through second 43.
Each sender completed 31,373 packets without a send error. A separate idle-flow
test retained one hardware entry through second 27 and removed it at second 28,
while its conntrack entry remained present; this matches the configured
30-second `nf_flowtable_udp_timeout` after accounting for the delay between the
last packet and the first sample.

The real-statistics lifecycle was then repeated on the final FIT. A bidirectional
UDP connection remained `[HW_OFFLOAD]` at 5, 30, and 60 seconds while 115,587
packets crossed each direction. A separately timed idle connection remained in
hardware through second 28, lost the marker by second 34, and kept its
conntrack entry. One hundred add/stats/delete/reuse cycles completed without a
failure or stale counter.

A 256-connection flower test programmed 512 hardware directions. Eight
connections distributed across the allocation each reported exactly 825
packets and 1,252,350 hardware bytes in both directions; eight untouched
connections remained at zero. Deleting all 256 connections left no LAN or WAN
rule, and dmesg remained clean.

With four other flows active, `nft flush ruleset` changed the dedicated-port
count from four hardware entries to zero without removing the four conntrack
entries. Reloading the ruleset did not retroactively offload those existing
connections, but a new connection immediately reached `[HW_OFFLOAD]`. This
matches the driver's ZCAM, IKEY, age-index, and one-WANID SNAT cleanup, and
confirms that subsequent allocation succeeds. Existing connections must be
recreated if the complete nftables flowtable is destroyed and created again.

## Bridge, Access-VLAN, and Software-Fallback Acceptance

Moving `192.168.5.1/24` from `lan1` to a VLAN-unaware `br0` did not change the
offload result. The DSA port joined the bridge, Windows remained reachable, and
an established UDP NAT connection through the bridge reached `[HW_OFFLOAD]`.
This confirms that the netfilter route actions still resolve the physical DSA
egress needed by the NPPT response.

The same bridge was then changed to VLAN-aware mode with default PVID 0 and
customer VID 100 configured as PVID/untagged on both `lan1` and the bridge self
port. Windows stayed untagged and reachable. A UDP NAT connection on port 5700
again reached `[HW_OFFLOAD]` with bidirectional delivery. This is the supported
access-VLAN model: the customer VLAN is nested inside the private DSA CPU-link
transport, and tagged trunk VLANs remain outside the current driver scope.

ICMP is intentionally absent from the nftables `flow add` rule. Four Windows
pings crossed the VLAN-aware bridge and SNAT path with zero loss and 1 ms
reported latency; the ICMP conntrack entry contained no `[HW_OFFLOAD]` marker.
This verifies ordinary software forwarding for a protocol outside the hardware
backend rather than dropping it or falsely claiming offload. No debugfs or
production-register interface was added; the one-off userspace netlink helper
used to configure VID 100 was removed from host and board after the test. The
kernel log remained free of BUG, Oops, WARNING, WANID, and SMMU failures.
