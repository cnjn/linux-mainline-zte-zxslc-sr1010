# ZX279133 Network Resource and Evidence Map

## Purpose

This document is the Phase 0 hardware-resource baseline for the ZX279133
mainline network adaptation. It relates vendor device-tree data, recovered
`plat_132.ko` behavior, captured SR1010 runtime state, and the current
`linux-6.18.38/` representation.

This is not a proposed binding by itself. Driver ownership and DT layout remain
subject to the decisions recorded below.

## Evidence Labels

- **Verified**: supported by at least two independent evidence classes, normally
  recovered binary behavior plus vendor DT or runtime state.
- **Strong**: directly present in one primary source and consistent with other
  observations, but not yet exercised or captured independently.
- **Inference**: a plausible interpretation of a directly observed value or
  access pattern. It must not become a binding ABI without validation.
- **Unknown**: insufficient evidence for an implementation decision.

The current mainline tree is implementation context only. It does not raise the
confidence of a hardware claim.

## Primary Evidence

- Vendor DT: `vendor-reference/2b5/zx279133-sr1010.dts`, especially lines
  1558-1634.
- Probe behavior: `docs/reverse-engineering/plat_132/recovered/plat_probe.c`,
  lines 99-209.
- Clock, reset, SerDes, XPCS, and XMAC behavior:
  `docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
- IDM behavior: `docs/reverse-engineering/plat_132/recovered/plat_idm.c`.
- Interrupt runtime state:
  `vendor-reference/sr1010-vendor-runtime/system/proc/interrupts`.
- Clock runtime state:
  `vendor-reference/sr1010-vendor-runtime/debugfs/clk_summary.txt`.
- Bring-up runtime state:
  `vendor-reference/sr1010-vendor-runtime/kernel/dmesg.txt`.
- Current implementation target:
  `linux-6.18.38/arch/arm64/boot/dts/zte/zx279133.dtsi` and
  `linux-6.18.38/drivers/clk/clk-zx279133.c`.

## MMIO Resource Matrix

| Block | Physical range from vendor DT | Recovered owner/use | Current mainline representation | Confidence | Required action |
| --- | --- | --- | --- | --- | --- |
| PON core | `0x17000000..0x17ffffff` | `pon_base`, PON interrupt mask/status, PON-to-XMAC1 routing, PON protocol callbacks | Absent | Verified | Defer the full node until PON work; retain only dependencies needed by a demonstrated P2P path |
| SoC sysctrl | `0x10e00000..0x10e00fff` | `sys_ctrl_base`; writes `0x00200020` to offsets `0x78` and `0x7c` in `nppt_idm_cci_enable()` | Absent | Verified address/access; inference for bit meaning | Model through a specific syscon interface only after CCI semantics and ownership are established |
| TOPCRM aggregate vendor window | `0x10e10000..0x10e1ffff` | `top_crm_base`; network muxes, gates, resets, and PON PLL programming | Split among `topcrm` at `+0x00..0x5f`, `top_reset` at `+0x60`, and `local_reset` at `+0x70`; `+0xc0/+0xc4` absent | Verified | Preserve non-overlapping ownership; determine how PON PLL registers are represented before extending any node |
| Pinmux | `0x10e20000..0x10e20fff` | `pin_mux_base`; also manually aliased as `lowpower_gpio_config_base` for `0x200` bytes | `pinctrl@10e20000` covers `0x1000` | Verified | Reuse pinctrl; add only evidenced network pin groups |
| Efuse | `0x14f11000..0x14f11fff` | `efuse_base`; SerDes/PHY calibration and SoC data reads | `efuse@14f11000` covers `0x1000` | Verified | Expose required values through nvmem cells, not direct cross-driver MMIO |
| PPS | `0x18000000..0x18ffffff` | `pps_base`; SE parser, SMMU, hash, and packet processing | Second NPPT `reg` resource named `pps` | Verified | NPPT currently owns both forwarding MMIO windows; split only with a complete independent PPS driver |
| NPPT aggregate | `0x19000000..0x19ffffff` | `nppt_base`; interrupt controller, SMAC/XMAC controls, packet processor, and IDM at offset `0x280000` | Disabled `ethernet@19000000`, one `0x1000000` region, five named IRQs, six clocks, and XPCS1 reference | Verified | Keep one aggregate MMIO owner; add PHY and syscon/route dependencies only with their owning drivers |
| IDM sub-block | `0x19280000` (`NPPT + 0x280000`) | `IDM_REG_BASE`; RX/TX rings, queue control, doorbells, masks, and completion state | Covered by NPPT MMIO without an overlapping node; all four IDM IRQs are present on the Ethernet node | Verified base; unknown formal sub-block size | Keep using the aggregate NPPT mapping until a real hardware boundary is proven |
| RGMII | `0x15900000..0x159fffff` | `rgmii_base`; RGMII and GEPHY-related controls | Absent | Verified | Defer unless Phase 0.2 selects an RGMII/GEPHY path |
| XPCS aggregate | `0x1a000000..0x1bffffff` | `xmac0_pcs_base`; `xpcs_register_for_xmac()` selects an instance with `xmac << 24` | Two disabled `snps,dw-xpcs` nodes at `0x1a000000` and `0x1b000000`, each sized `0x800000`; XPCS1 uses PON SerDes PCLK | Verified | Retain separate instances; `0x800000` covers the complete direct Clause 45 address encoding |
| PON SerDes | `0x16000000..0x1600ffff` | `pon_serdes_base`; PON and P2P SerDes profiles, PLL/CDR polling | Disabled `phy@16000000`, first `serdes` resource | Verified | Implement as the selected Generic PHY provider |
| PON SerDes PLL | `0x16010000..0x1601ffff` | `pon_serdes_pll_base`; companion PLL access | Second `pll` resource on the disabled PON SerDes PHY | Verified | Owned exclusively by the PON SerDes PHY driver |
| Uni SerDes | `0x16100000..0x161fffff` | `uni_serdes_base`; Uni SerDes mode setup and lock checks; first `0x200` bytes are also manually aliased for low-power configuration | Absent | Verified | Add a generic PHY provider; use one owner for the overlapping low-power alias |
| PCU | `0x10e30000..0x10e3ffff` | `pcu_base`; recovered platform control routines | Absent | Strong | Determine whether the first Ethernet path needs it before adding a DT dependency |
| GEPHY APB | `0x15400000..0x158fffff` | `gephy_apb_base`; internal GEPHY control/calibration | Absent | Verified address; strong usage | Defer unless Phase 0.2 selects the GEPHY path |
| MDIO0 | `0x14f01000..0x14f01fff` | Separate vendor MDIO platform device | Disabled `mdio@14f01000`; driver, clocks, and reset exist | Verified | Enable on the board only if required by the selected path |
| MDIO1 | `0x14f02000..0x14f02fff` | Separate vendor MDIO platform device | Disabled `mdio@14f02000`; driver, clocks, and reset exist | Verified | Enable on the board only if required by the selected path |
| Low-power config alias | `0x10e10000..0x10e101ff` | Manual mapping named `lowpower_config_base` | Overlaps TOPCRM and reset ranges | Verified alias; unknown ownership requirement | Never describe as a second DT MMIO owner; route accesses through the owning subsystem |
| Low-power XMAC alias | `0x16100000..0x161001ff` | Manual mapping named `lowpower_xmac_config_base` | Would overlap Uni SerDes | Verified alias; unknown ownership requirement | Keep under the Uni SerDes owner unless later evidence proves a distinct block |
| Low-power GPIO alias | `0x10e20000..0x10e201ff` | Manual mapping named `lowpower_gpio_config_base` | Overlaps pinctrl | Verified alias; unknown ownership requirement | Use pinctrl or a narrowly defined shared interface; never add an overlapping node |

### MMIO Notes

- The vendor PON node supplies five mappings in this exact order: PON,
  sysctrl, TOPCRM, pinmux, and efuse. `zx_pon_probe` consumes indexes 0 through
  4 in the same order.
- The vendor XPCS node has one `0x2000000` mapping at `0x1a000000`.
  `xpcs_register_for_xmac()` adds `(xmac << 24)`, directly supporting XPCS0 at
  `0x1a000000` and XPCS1 at `0x1b000000` for XMAC IDs 0 and 1.
- XMAC/SMAC control registers also live inside NPPT. Examples include the SMAC
  reset at NPPT offset `0x2c0004`, SOPC registers near `0x34000`, and per-SMAC
  blocks using a `0x40000 * (mac + 1)` stride. PCS and NPPT control are therefore
  distinct MMIO domains even when one MAC driver coordinates them.
- The formal IDM register aperture is unknown. Recovered accesses reach at
  least IDM offset `0x5c0`, but this lower bound must not be used as a binding
  size. Hardware validation also showed that reading
  IDM MMIO after `pon_idm_aclk` is gated causes a watchdog reset. Diagnostics
  and driver accesses must therefore remain inside the clock-enabled lifetime.

## Interrupt Matrix

| Logical interrupt | Vendor DT hwirq | Trigger | Recovered handler/name | Runtime confirmation | Current mainline |
| --- | --- | --- | --- | --- | --- |
| PON aggregate | GIC SPI 6 | Level high | `zx_pon_int`, requested as `pon` | GIC hwirq 38, Linux IRQ 21, `pon` | Absent |
| NPPT aggregate | GIC SPI 5 | Level high | `zx_nppt_int`, requested as `nppt` | GIC hwirq 37, Linux IRQ 25, `nppt` | Present as `nppt` |
| IDM CPU RX | GIC SPI 1 | Level high | `idm_cpu_int`, name `cpu` | GIC hwirq 33, Linux IRQ 26, active RX counts observed | Present as `rx` |
| IDM companion/Wi-Fi path | GIC SPI 2 | Level high | `idm_wifi_int`, name `idm` | GIC hwirq 34, Linux IRQ 27 | Present as `idm` |
| IDM buffer release | GIC SPI 3 | Level high | `idm_rls_int`, name `buf_rls` | GIC hwirq 35, Linux IRQ 28 | Present as `buffer-release` |
| IDM local test | GIC SPI 4 | Level high | `idm_all_int`, name `localtest` | GIC hwirq 36, Linux IRQ 29 | Present as `local-test` |
| PPS | GIC SPI 61 and 62 in vendor DT | Level high | Probe consumes only interrupt index 0 as `g_pps_irq` | Not active in the capture | Absent |

All network interrupt trigger types above are **Verified** from vendor DT plus
runtime `/proc/interrupts`, except PPS, which is **Strong** because the node was
disabled in the captured runtime.

The vendor CPU affinity policy is not hardware topology. The mainline driver
must not encode the captured two-core affinity assignments as DT data.

## Clock and Reset Dependencies

### Existing CCF Coverage

| Clock | Recovered or runtime evidence | Current implementation | Confidence |
| --- | --- | --- | --- |
| PON/NPPT WCLK mux | TOPCRM offset `0x0c`, bits 24-26; runtime rate `125 MHz` | `ZX279133_TOPCRM_CLK_PON_NPPT_WCLK_MUX`, read-only mux | Verified |
| PON WOE1 WCLK | TOPCRM offset `0x48`, bit 10; selected parent rate `416.666 MHz` | `ZX279133_TOPCRM_CLK_PON_WOE1_WCLK` | Verified |
| Uni SerDes PCLK | TOPCRM offset `0x44`, bit 8; runtime rate `125 MHz` | `ZX279133_TOPCRM_CLK_UNI_SERDES_PCLK` | Verified |
| PON SerDes PCLK | TOPCRM offset `0x44`, bit 0; parent `sys_pclk`, runtime rate `125 MHz` | `ZX279133_TOPCRM_CLK_PON_SERDES_PCLK`; shared by PON SerDes and XPCS1 | Verified |
| IDM/TM ACLK | TOPCRM offset `0x48`, bits 0/1; parent `sys_aclk`, runtime rate `250 MHz` | `ZX279133_TOPCRM_CLK_PON_IDM_ACLK` / `PON_TM_ACLK` | Verified |
| NPPT PCLK | TOPCRM offset `0x48`, bit 2; parent `sys_pclk`, runtime rate `125 MHz` | `ZX279133_TOPCRM_CLK_PON_PCLK` | Verified |
| SMAC working clock | TOPCRM offset `0x48`, bit 7; parent `clk125m` | `ZX279133_TOPCRM_CLK_PON_SMAC_WCLK` | Verified |
| XMAC working clock | TOPCRM offset `0x48`, bit 12; parent `clk250m` | `ZX279133_TOPCRM_CLK_PON_MAC_WCLK` | Verified |

The disabled NPPT Ethernet node now lists all six demonstrated packet-datapath
gates. It does not claim unresolved PON protocol or offload clocks.

### Missing or Unresolved Network Clocks

| Vendor clock/function | Recovered operation | Runtime clock name/rate | Confidence and issue |
| --- | --- | --- | --- |
| PON/NPPT core selection | TOPCRM `0x0c`, bits 4-6 are set by `pon_soc_pon_core_clk_init()` on ZX279133 | `pon_core_clk_mux` / `pon_core_wclk`, about `688.128 MHz` | Strong; distinguish mux selection from gate semantics |
| PON CCI clock | TOPCRM `0x04`, bits 4-5 or 4-6 are set by CCI initialization | `cci_aclk`, `500 MHz` | Strong; shared with system CCI and requires ownership analysis |
| WOE0 WCLK | TOPCRM `0x0c`, bits 20-22 set by recovered code | `pon_woe0_wclk`, `400 MHz` | Strong; may not be needed for minimal P2P |
| Uni SerDes reference path | Present as `uni_serdes_50m_clk` but captured summary reports `500 MHz` | `uni_serdes_50m_clk`, reported `500 MHz` | Unknown; name/rate conflict requires register and hardware validation |
| RGMII clock | Recovered TOPCRM `0x0c` bit 16 control | `pon_rgmii_clk`, `250 MHz` | Strong; deferred unless selected path needs it |
| GEPHY PCLK | Present in vendor CCF runtime | `gephy_pclk`, reported `500 MHz` | Strong existence; exact field and expected rate unresolved |

### Reset and One-Time Configuration

| Target | Recovered operation | Current representation | Confidence and action |
| --- | --- | --- | --- |
| PON/SerDes local reset | TOPCRM offsets `0x70` bits 0 and 1 toggled with delays | Active-low reset IDs assigned for PON SerDes and APB/config reset | Verified |
| CPU132-only PON reset | TOPCRM offset `0x60`, bit 9 toggled only in the CPU132 branch | `top_reset@10e10060` exists | Not consumed on ZX279133 CPU133 selected path |
| PON PLL configuration | TOPCRM offsets `0xc0` and `0xc4`, plus selection at `0x10` | Not represented | Verified access; should not enlarge the CCF MMIO range across existing reset nodes |
| NPPT/SMAC soft reset | NPPT offset `0x2c0004`, bit 31 is cleared then set | Covered by aggregate NPPT region, no reset controller | Verified; keep internal to NPPT driver unless multiple independent consumers require a reset controller |
| IDM CCI enable | Sysctrl offsets `0x78` and `0x7c` receive `0x00200020` | Sysctrl absent | Verified writes but inferred bit meaning; model as documented one-time SoC configuration, not automatically as a clock |
| PON-to-XMAC1 route | PON offset `0x80` and NPPT offset `0x2438` bit 2 | Both aggregate blocks, PON absent | Verified; Phase 0.2 must determine whether the selected P2P path requires this cross-block route |

## DMA and Reserved Memory Evidence

- IDM contains 24 RX queues and four TX queues.
- Both RX and TX descriptors are 32 bytes. The recovered allocator lays out RX
  descriptors first, followed by TX descriptors and buffer-pointer rings.
- Vendor runtime reserved `0xa00000` bytes starting at physical `0x9d700000`.
  `idm_init()` calculated `0x813800` bytes required in the captured boot.
- The vendor Linux early-memory split is contiguous and exact: IDM
  `0x9d700000/0x0a00000`, SE hash `0x9e100000/0x1000000`, and BMU
  `0x9f100000/0x0f00000`. `tm_bmu_init()` requires `0x0ecc000` bytes inside the
  BMU region.
- The SR1010 DT now reserves SE hash `0x9e100000/0x01000000` and BMU
  `0x9f100000/0x00f00000` as adjacent `no-map` regions. NPPT receives them in
  the ordered `memory-region-names = "bmu", "se-hash"` list and probe validates
  both minimum sizes. SE hash remains unmapped and uncleared until its complete
  SMMU/hash initialization contract is implemented.
- Vendor code derives linear-map virtual addresses and programs 32-bit physical
  addresses directly. BMU sub-layout recovery proves that its fixed region is a
  hardware-visible pool, unlike ordinary IDM packet DMA mappings.
- FIT SHA256 `6bad3ec8c0b704136050838ee0f6c33535e89dbbd3acb241b1a0ad6750a36f57`
  confirms `/proc/iomem` reserves `0x9f100000..0x9fffffff` as one 15 MiB,
  non-reusable `no-map` range and the NPPT driver validates its size.

**Mainline decision:** retain the 32-bit DMA API for IDM descriptors and packet
mapping. The independently addressed BMU region is DT-owned reserved memory;
`tm_bmu_init()` uses its exact internal pool layout and never substitutes
arbitrary DMA allocations.

The exact descriptor field meanings and coherency behavior remain outside Phase
0.1 and are tracked by Phase 5.1.

## Current DT Gaps

The selected TX path now has SerDes, XPCS, PHY, route, IDM CCI, and BMU
reserved-memory ownership. Remaining DT work is limited to resources proven by
later stages, notably any SE hash memory required by parsing/classification and
the eventual IDM RX memory model.

The stale generated Uni SerDes and XMAC PCS binding artifacts in
`Documentation/devicetree/bindings/` are not source and must not be used to fill
these gaps.

## Candidate Ownership for the First Driver Series

This is a working model, not a binding commitment:

1. TOPCRM CCF and existing reset-controller nodes own their non-overlapping
   clock/reset registers.
2. A ZX279133 generic PHY provider owns one selected SerDes region and its
   evidenced PLL/configuration dependencies.
3. Generic `snps,dw-xpcs` owns each validated XPCS instance aperture.
4. One NPPT/IDM Ethernet driver owns the NPPT aggregate mapping, NPPT plus four
   IDM interrupts, DMA rings, NAPI, XMAC controls, and the initial netdev.
5. MDIO and PHYLIB own an external PHY if Phase 0.2 places it on the first path.
6. PON, PCU, GEPHY, and RGMII remain absent from the initial binding unless the
   selected physical path proves they are required.

## Remaining Resource Questions

- Does IDM operate coherently under the mainline kernel, or are explicit DMA
  sync operations required?
- Which minimum `np.ko` forwarding operations are required before CPU-to-XMAC
  traffic can pass?

## Phase 0.1 Result

Phase 0.1 is complete at documentation level: every resource currently proposed
for the networking effort has an evidence source, a confidence label, and a
current DT comparison. No kernel DT or driver ABI should be changed until Phase
0.2 selects the first physical path and Phase 1.1 resolves TOPCRM ownership.
