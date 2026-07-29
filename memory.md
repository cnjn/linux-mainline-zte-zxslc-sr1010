# Session Memory

## Current Goal

The user has completed the semantic reconstruction of `plat_132.ko` and asked
which parts can support a mainline Linux port for the ZX279133 SoC. This turn
performed an assessment only; no kernel source was changed.

The durable conclusion is that `plat_132` is a strong specification for the
ZX279133 SoC-side networking stack, especially P2P WAN bring-up, but it is not a
complete specification for the full PON, packet-processing, PHY, or external
switch stack.

## Reconstruction State

- Target module: `vendor-reference/sr1010-vendor-runtime/modules/files/kmodule/plat_132.ko`.
- Reconstruction documents: `docs/reverse-engineering/plat_132/`.
- `COMPLETENESS.md` records 559 independent internal functions, all covered by
  function records and nine source-like C units under `recovered/`.
- The recovered C is semantic pseudocode, not a buildable replacement module.
- Remaining general blockers include opaque vendor types, unresolved descriptor
  bit meanings, raw MMIO names, companion-module policy, and hardware validation.
- Hard evidence boundary remains in force: `linux-6.18.38/` is mainline context,
  not evidence for vendor behavior. Vendor behavior must come from the module,
  runtime capture, and narrowly needed companion modules.

## Mainline Adaptation Assessment

### Strongly Supported

1. Device tree and resource topology
   - `recovered/plat_probe.c` establishes the MMIO resources and probe order for
     PON, NPPT, IDM IRQs, RGMII, XPCS, PON/Uni SerDes, PCU, GEPHY, sysctrl,
     TOPCRM, pinmux, and efuse.
   - Vendor hardware IRQs are PON GIC SPI 6, NPPT GIC SPI 5, and IDM GIC SPIs
     1 through 4 (`cpu`, `idm`, `buf_rls`, and `localtest`).
   - Runtime bases include NPPT `0x19000000`, IDM at NPPT offset `0x280000`,
     PON `0x17000000`, PON SerDes `0x16000000`, its PLL `0x16010000`, Uni
     SerDes `0x16100000`, and XPCS windows at `0x1a000000`/`0x1b000000`.

2. Network clocks and resets
   - Recovered code gives the exact ZX279133 TOPCRM mux/gate operations and
     reset ordering for NPPT, IDM, TM, CCI, WOE, PON core, XMAC, and SerDes.
   - The current clock driver already exposes Uni SerDes PCLK, the PON/NPPT
     WCLK mux, and PON WOE1 WCLK in `drivers/clk/clk-zx279133.c`.
   - Additional network gates/reset IDs can be modeled through CCF and reset
     controllers. Shared CCI/TOPCRM changes require hardware validation and
     must not be copied blindly from vendor RMW sequences.

3. SerDes, XPCS, and XMAC
   - `recovered/plat_smac.c` covers SGMII, 1000BASE-X, 2500BASE-X, HSGMII,
     USXGMII, 5GBASE-R, and 10GBASE-R, including PLL/CDR polling and runtime
     speed/duplex transitions.
   - Prefer a ZX279133 SerDes PHY driver plus the generic `snps,dw-xpcs`/phylink
     framework. Use recovered XPCS code to identify required quirks instead of
     duplicating all generic DesignWare PCS logic.
   - Captured SR1010 runtime uses XMAC work modes 5 and 4; these are the best
     initial board-specific modes to implement and validate.

4. IDM DMA, IRQ, NAPI, and basic packet I/O
   - `recovered/plat_idm.c`, `plat_cpu_net.c`, and `plat_cpu_tx.c` establish 24
     RX queues, four TX queues, 32-byte descriptors, ring allocation/layout,
     refill, TX completion, doorbells, IRQ masking, and four NAPI paths.
   - This is enough to build a basic single-WAN netdev prototype, but the
     mainline implementation must use the DMA API and proper lifecycle cleanup.
   - Vendor runtime reserved `0xa00000` bytes at `0x9d700000` and calculated a
     requirement of `0x813800`. The vendor uses `virt_to_phys` and linear-map
     assumptions; mainline should use a 32-bit coherent DMA mask and
     `dma_alloc_coherent()`/CMA unless fixed memory is proven necessary.

### Partially Supported

- PON: clock/reset, SerDes profiles, lock checks, work-mode selection, and the
  top-level PON/NPPT IRQ demultiplexers are recovered. Full GPON/EPON/XGPON MAC,
  ONU registration, DBA, GEM/LLID, OMCI/OAM, and optical policy are not.
- PHY integration: link-state-to-XMAC mode glue is recovered, but the ZX279051
  PHY's calibration, patch, and Clause 45 behavior belong to `zx279051.ko`.
- Switch integration: callback boundaries are documented in
  `CALLBACK_INTERFACES.md`, but RTL8372N and switch policy belong to switch
  modules and require a separate DSA implementation.
- PTP/PPS: interrupt bits and callback dispatch are known, but there is not
  enough local implementation for a PHC/PTP driver.
- APB Timer0/1 is reconstructed in `recovered/plat_timer.c`, but the mainline
  DTS already uses the ARM architected timer, so this is not a boot priority.

### Not Established by plat_132 Alone

- NPPT forwarding-plane policy and initialization owned by `np.ko`, including
  the required subset of TM, QMG, FFE, WOE, BMU, classifier, and forwarding
  configuration. MAC link and IDM rings do not by themselves guarantee ping.
- ZX279051 PHY internals.
- RTL8372N external switch control and DSA topology.
- Full PON line protocols and management plane.
- Generic SoC peripherals such as GPIO, SFC, USB, watchdog, PVT, and UART;
  these are outside `plat_132` and are already represented separately in the
  current mainline tree.

## Current Mainline Tree Snapshot

- `arch/arm64/boot/dts/zte/zx279133.dtsi` contains the basic SoC, GICv3, ARM
  timer, clocks, UART, GPIO, SFC, USB, MDIO, NPPT, and two XPCS nodes.
- NPPT and both XPCS nodes are currently disabled. The NPPT binding exposes
  only one MMIO range, one IRQ, and one clock, which is insufficient for the
  recovered aggregate Ethernet datapath.
- The SR1010 board DTS does not currently enable MDIO, NPPT, XPCS, or a
  functional Ethernet node.
- At inspection time, `out/kernel` retained compiled `zx279133.o` and
  `zx279051.o` prototype artifacts and a config enabling their symbols, while
  the corresponding source files/Kconfig entries were absent from the visible
  source tree. Treat these as stale build artifacts, not current source truth.

## Recommended Mainline Decomposition

- Extend ZX279133 clock/reset definitions for network consumers.
- Add a ZX279133 Uni/PON SerDes PHY provider.
- Reuse generic DesignWare XPCS, adding only demonstrated quirks.
- Add a ZTE NPPT/IDM Ethernet driver using phylink, DMA API, NAPI, and one
  physical WAN netdev first.
- Do not reproduce the vendor `sw`, `pon`, `oam`/`omci`, and `idm` pseudo-netdev
  architecture or unsynchronized callback slots as the upstream interface.
- Keep vendor GSO/GRO, upload hooks, testftp, debug procfs, and QoS callback
  machinery out of the initial driver. Use normal kernel networking first.
- Implement the ZX279051 PHY and RTL8372N DSA drivers as separate work items.
- Add PON protocol support only after the Ethernet/P2P path is stable and the
  required companion modules have been analyzed.

## Recommended Bring-up Order

1. Complete network DT resources, clocks, resets, syscon references, and IRQ
   names.
2. Bring up SerDes and XPCS and verify link/PLL/CDR status without packet DMA.
3. Implement a single-WAN IDM netdev with basic RX/TX and no offloads.
4. Recover or implement the minimum NPPT forwarding setup needed to connect the
   CPU port to the chosen XMAC.
5. Integrate ZX279051 through PHYLIB/phylink and validate ping/iperf.
6. Add the RTL8372N DSA path, then optional PON/PTP/offload features.

The practical near-term target is P2P WAN Ethernet on SR1010: link up, ping,
then iperf. Full ONU/PON and LAN switching are independent later milestones.
