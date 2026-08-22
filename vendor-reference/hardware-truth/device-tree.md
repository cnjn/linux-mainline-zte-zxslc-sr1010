# ZX279133 device-tree hardware truth

## Scope and method

Compared (1) FIT/vendor DT source and blob, (2) captured live runtime DTB after U-Boot fixups, and (3) Linux 6.18.38 mainline-style DTSI + SR1010 board DTS. `dtc -I dtb -O dts` produced 1,779-line trees for both blobs. The FIT blob decompilation is structurally the same as `vendor-reference/2b5/zx279133-sr1010.dts`; the live tree differs in only three properties. Numeric interrupt cells are GIC tuples (`type=0` SPI, `type=1` PPI; final cell flags).

Evidence: `vendor-reference/2b5/zx279133-sr1010.dts`; `vendor-reference/2b5/zx279133-sr1010.dtb`; `vendor-reference/sr1010-vendor-runtime/device-tree/runtime.dtb`; `vendor-reference/sr1010-vendor-runtime/device-tree/proc-device-tree.tar`; `linux-6.18.38/arch/arm64/boot/dts/zte/zx279133.dtsi`; `linux-6.18.38/arch/arm64/boot/dts/zte/zx279133-sr1010.dts`.

## High-confidence conclusions

* Live U-Boot fixups are minimal: SFC changes from disabled to okay; `chosen.bootargs` changes to JFFS2 `/dev/mtdblock8`; `chosen.versioninfo` is populated. No hardware resource, address, interrupt, clock, reset, OPP, or wiring cells otherwise change.
* Vendor DT models a broad BSP inventory (three UARTs, two I2C, three I2S, PWM regulator + CPU OPPs, two USB, MMC, NAND, SFC, dual PCIe/MSI/Wi-Fi, SSP/TDM, PON/PPS/NPPT/RGMII/SerDes/PCU/GEPHY/IPsec/DDR helper nodes). Most data-plane and PCIe blocks are disabled, while PON, both watchdogs, PVT, UART0, I2C0/1, USB at 0x15010000, MDIO0/1 and DDR helper nodes are enabled.
* Mainline deliberately contracts the description to supported hardware: clocks/resets, UART0, PVT, GPIO, SFC, USB3, MDIO, PWM, efuse, watchdogs, unified PON+UNI SerDes/NPPT/XPCS. It omits vendor PCIe/Wi-Fi, I2C/audio/voice, MMC/raw NAND, PON standalone driver, RGMII/GEPHY/IPsec/DDR helper nodes, CPU DVFS/regulator, and all vendor leaf-clock nodes.
* Mainline board wiring adds reset key GPIO0[0] active-low; white LED GPIO2[0] active-low; red LED GPIO2[4] active-low; RTL8372N reset GPIO1[1] active-low and SMI address 0x1d; ZX279051 PHY address 1 reset GPIO0[1] active-low; four 2.5-Gbit internal LAN ports (4..7) and a 10-Gbit CPU port 8; three NPPT reserved-memory pools; SPI-NAND partitions; and a PVT thermal zone.
* No `dmas`/`dma-names`, IOMMU, power-domain, interconnect, or vendor thermal-zone bindings occur in either vendor tree. Mainline also has no DMA/IOMMU/power-domain/interconnect references; its only thermal consumer is the board `soc-thermal` zone.

## FIT versus live runtime

| Path/property | FIT value | Live value | Evidence |
|---|---|---|---|
| `/soc/spifc@0x10d0f000/status` | `disabled` | `okay` | vendor source lines 1312-1323; runtime DTB |
| `/chosen/bootargs` | `console=ttyAMA0,115200n8 rdinit=/sbin/init mem=512M ` | `console=ttyAMA0,115200n8 root=/dev/mtdblock8 ro rootfstype=jffs2 mem=512M serial=open pcie_pme=nomsi` | vendor source lines 1660-1666; runtime DTB |
| `/chosen/versioninfo` | one zero byte | large firmware-populated board/version data block | vendor source lines 1660-1666; runtime DTB |

Thus the FIT table below is also the live table except for the SFC status and chosen metadata.

## Important cross-tree anomalies and semantic changes

1. **Root compatible mismatch:** vendor says only `zte,133` (line 8), whereas mainline uses `zte,zx279133` and board `zte,zx279133-sr1010` (DTSI line 10; DTS lines 10-12).
2. **SFC:** vendor maps 0x10d0f000 size 0x40000, one WCLK, vendor `spi-chipselect`, 100 MHz and dynamic partitions (lines 1312-1339). Mainline maps only 0x1000, adds PCLK+WCLK and reset, uses `zte,spi-chipselect`, fixed partitions, and board caps both controller/flash at 25 MHz (DTSI 252-317; board 222-237). Live firmware enables the vendor SFC.
3. **PVT:** vendor has four SPIs 190..193 and a single leaf clock, efuse bit coordinates embedded in vendor properties (1019-1027). Mainline has no PVT IRQ, uses two explicit top clocks and an nvmem cell at efuse offset 0x44 bits 10:5 (DTSI 167-176, 380-397).
4. **Watchdogs:** vendor exposes SPI 53/54 and only WDT clocks, both okay (1141-1157). Mainline omits watchdog IRQs, supplies PCLK+WDTCLK and explicit reset syscon/masks; WDT0 is always okay and board enables WDT1 (DTSI 400-424; board 218-220).
5. **USB:** vendor has controllers at 0x15008000/SPI63 disabled and 0x15010000/SPI65 okay, generic-xhci and two clocks (1225-1241). Mainline retains only the latter, uses SoC compatible and three clocks (`core`,`reg`,`cci`) (DTSI 322-331; board 95-97).
6. **NPPT composition:** vendor splits PON 0x17000000 (enabled), PPS 0x18000000 and NPPT 0x19000000 (disabled), with standalone SerDes and XPCS coverage 0x1a000000 size 0x02000000 (1559-1633). Mainline NPPT combines NPPT+PPS regs, five interrupts (SPI 5,1,2,3,4), six clocks, two PHYs and two XPCS handles; board enables all and adds memory pools and switch fabric (DTSI 427-508; board 103-215).
7. **SerDes:** vendor models PON SerDes and PLL as separate disabled nodes and has no PHY/reset/clock bindings (1586-1600). Mainline combines PON SerDes, PLL, and TOPCRM mode registers with named clock/resets, plus UNI SerDes clock/resets (DTSI 427-458).
8. **XPCS aperture:** vendor one node at 0x1a000000 size 0x02000000 (1631-1633); mainline splits XPCS0 0x1a000000/0x800000 and XPCS1 0x1b000000/0x800000 (DTSI 491-508).
9. **CPU DVFS removed:** vendor has shared OPPs 500/688/900/1000/1100/1200/1300 MHz at 842/850/858/858/865/873/873 mV (lines 603-653), CPUs reference a PWM regulator and OPP table (698-719), regulator at 1205-1215. Mainline fixes CPU PLL at 2 GHz and omits OPP/regulator references pending safe voltage scaling (DTSI 43-49, 55-69).
10. **PCIe DT defects:** both vendor PCIe MHI child `reg` properties are 20 bytes under inherited 2/1 address/size cells; `dtc` reports invalid `reg_format` (vendor 1381-1463). PCIe/MSI remain disabled. Other schema defects include unit addresses with `0x`, nodes with `reg` but no unit address, OPP unit addresses without `reg`, misspelled `clocks-names` on SSP, binary/string-rendered numeric properties (`regulator-min-microvolt`, SSP1 frequency), and `status="ok"` rather than canonical `okay`.
11. **Flash overlap by design/vendor ambiguity:** vendor dynamic layouts alias `kernel1` and `rootfs1` to the same 0x600000+0x2900000 region, and likewise slot2 at 0x2f00000 (1673-1777). Mainline renames each combined region `firmware-slot1/2`, avoiding overlapping child partitions (DTSI 301-311).
12. **Reserved memory only in mainline board:** 0x9d700000/10 MiB IDM, 0x9e100000/16 MiB SE hash, 0x9f100000/15 MiB BMU are `no-map` and attached to NPPT (board 29-47, 111-116). Vendor DDR helper nodes carry no `reg`/memory-region, so their actual carveouts are not described.

## Vendor FIT normalized resource inventory

Raw phandles are retained because the vendor source is a decompilation. Status absent means enabled by DT convention, though a driver may still depend on firmware/platform support.
| Node (source line) | compatible / status | resources & wiring |
|---|---|---|
| `/` (L3) | `compatible="zte,133"` | — |
| `/top_fixed` (L10) | `compatible="simple-bus"` | `ranges=true` |
| `/top_fixed/osc25m` (L16) | `compatible="zxic,zx-fixed-rate", "fixed-clock"` | `clock-frequency=<0x17d7840>` |
| `/top_fixed/osc50m_clk` (L24) | `compatible="zxic,zx-fixed-rate", "fixed-clock"` | `clock-frequency=<0x2faf080>` |
| `/fixed_div` (L32) | `compatible="simple-bus"` | `ranges=true` |
| `/fixed_div/sd_wclk_div` (L38) | `compatible="zxic,zx-fixed-factor"` | — |
| `/fixed_div/nand_wclk` (L47) | `compatible="zxic,zx-fixed-factor"` | — |
| `/top_pll` (L59) | `compatible="simple-bus"` | `ranges=true` |
| `/top_pll/pll_2000m_clk` (L65) | `compatible="zxic,zx-pll"` | `reg=<0x00 0x10e10080 0x00 0x10>`<br>`clocks=<0x02>`<br>`clock-frequency=<0x77359400>` |
| `/top_pll/pll_lsp_2000m_clk` (L78) | `compatible="zxic,zx-pll"` | `clocks=<0x02>`<br>`clock-frequency=<0x77359400>` |
| `/top_pll/pll_1376m_clk` (L88) | `compatible="zxic,zx-pll"` | `clocks=<0x02>`<br>`clock-frequency="R\b", ""` |
| `/top_pll/pll_fpp_2500m_clk` (L98) | `compatible="zxic,zx-pll"` | `clocks=<0x02>`<br>`clock-frequency=<0x9502f900>` |
| `/top_mux` (L109) | `compatible="simple-bus"` | `ranges=true` |
| `/top_mux/mux@0` (L115) | `compatible="zxic,zx-mux"` | `reg=<0x00 0x10e10000 0x00 0x04>` |
| `/top_mux/mux@1` (L125) | `compatible="zxic,zx-mux"` | `reg=<0x00 0x10e10004 0x00 0x04>` |
| `/top_mux/mux@2` (L138) | `compatible="zxic,zx-mux"` | `reg=<0x00 0x10e10008 0x00 0x04>` |
| `/top_mux/mux@3` (L151) | `compatible="zxic,zx-mux"` | `reg=<0x00 0x10e1000c 0x00 0x04>` |
| `/top_mux/mux@4` (L165) | `compatible="zxic,zx-mux"` | `reg=<0x00 0x10e10010 0x00 0x04>` |
| `/top_divider` (L176) | `compatible="simple-bus"` | `ranges=true` |
| `/top_divider/top_clk_div0` (L182) | `compatible="zxic,zx-div"` | `reg=<0x00 0x10e10058 0x00 0x04>` |
| `/top_divider/top_clk_div1` (L192) | `compatible="zxic,zx-div"` | `reg=<0x00 0x10e1005c 0x00 0x04>` |
| `/top_gate` (L203) | `compatible="simple-bus"` | `ranges=true` |
| `/top_gate/top_clk_gate0` (L209) | `compatible="zxic,zx-gate"` | `reg=<0x00 0x10e10030 0x00 0x04>` |
| `/top_gate/top_clk_gate2` (L221) | `compatible="zxic,zx-gate"` | `reg=<0x00 0x10e10038 0x00 0x04>` |
| `/top_gate/top_clk_gate3` (L234) | `compatible="zxic,zx-gate"` | `reg=<0x00 0x10e1003c 0x00 0x04>` |
| `/top_gate/top_clk_gate4` (L247) | `compatible="zxic,zx-gate"` | `reg=<0x00 0x10e10040 0x00 0x04>` |
| `/top_gate/top_clk_gate5` (L260) | `compatible="zxic,zx-gate"` | `reg=<0x00 0x10e10044 0x00 0x04>` |
| `/top_gate/top_clk_gate6` (L272) | `compatible="zxic,zx-gate"` | `reg=<0x00 0x10e10048 0x00 0x04>` |
| `/lsp0_clk` (L285) | `compatible="simple-bus"` | `ranges=true` |
| `/lsp0_clk/timer0_clk` (L295) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp0_clk/uart0_clk` (L307) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp0_clk/uart1_clk` (L317) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp0_clk/uart2_clk` (L327) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp0_clk/spifc_clk` (L337) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/mdio0_clk` (L356) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/mdio1_clk` (L368) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/i2c0_clk` (L380) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/i2c1_clk` (L391) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/ssp0_clk` (L402) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/ssp1_clk` (L414) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/zsi0_clk` (L426) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/zsi1_clk` (L436) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/isi0_clk` (L446) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/isi1_clk` (L457) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/tdm0_clk` (L468) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/tdm1_clk` (L480) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/wdt0_clk` (L492) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/wdt1_clk` (L502) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/wdt2_clk` (L512) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/wdt3_clk` (L521) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/i2s0_clk` (L530) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/i2s1_clk` (L540) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/i2s2_clk` (L550) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/pwm_clk` (L560) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/lsp1_clk/efuse_clk` (L570) | `compatible="zxic,zx-clk-lsp-regmap"` | — |
| `/opp_table0` (L602) | `compatible="operating-points-v2"` | — |
| `/opp_table0/opp@500000000` (L607) | — | `opp-hz=<0x00 0x1dcd6500>`<br>`opp-microvolt=<0xcd910>` |
| `/opp_table0/opp@688000000` (L614) | — | `opp-hz=<0x00 0x29020c00>`<br>`opp-microvolt=<0xcf850>` |
| `/opp_table0/opp@900000000` (L620) | — | `opp-hz=<0x00 0x35a4e900>`<br>`opp-microvolt=<0xd1790>` |
| `/opp_table0/opp@1000000000` (L626) | — | `opp-hz=<0x00 0x3b9aca00>`<br>`opp-microvolt=<0xd1790>` |
| `/opp_table0/opp@1100000000` (L632) | — | `opp-hz=<0x00 0x4190ab00>`<br>`opp-microvolt=<0xd32e8>` |
| `/opp_table0/opp@1200000000` (L639) | — | `opp-hz=<0x00 0x47868c00>`<br>`opp-microvolt=<0xd5228>` |
| `/opp_table0/opp@1300000000` (L646) | — | `opp-hz=<0x00 0x4d7c6d00>`<br>`opp-microvolt=<0xd5228>` |
| `/cpus/idle-states/cpu-sleep-0` (L675) | `compatible="arm,idle-state"` | — |
| `/cpus/idle-states/cluster-sleep-0` (L685) | `compatible="arm,idle-state"` | — |
| `/cpus/cpu@0` (L696) | `compatible="arm,cortex-a53", "arm,armv8"` | `reg=<0x00 0x00>`<br>`clocks=<0x07 0x00>`<br>`operating-points-v2=<0x0b>`<br>`cpu-supply=<0x0a>`<br>`clock-frequency=<0x3b9aca00>` |
| `/cpus/cpu@1` (L709) | `compatible="arm,cortex-a53", "arm,armv8"` | `reg=<0x00 0x01>`<br>`clocks=<0x07 0x00>`<br>`operating-points-v2=<0x0b>`<br>`cpu-supply=<0x0a>`<br>`clock-frequency=<0x3b9aca00>` |
| `/psci` (L723) | `compatible="arm,psci-0.2", "arm,psci"` | — |
| `/interrupt-controller@00500000` (L733) | `compatible="arm,gic-v3"` | `reg=<0x00 0x500000 0x00 0x10000 0x00 0x540000 0x00 0x80000>`<br>`interrupts=<0x01 0x09 0x04>` |
| `/timer` (L742) | `compatible="arm,armv8-timer"` | `interrupts=<0x01 0x0d 0x308 0x01 0x0e 0x308 0x01 0x0b 0x308 0x01 0x0a 0x308>` |
| `/memory@80000000` (L747) | — | `reg=<0x00 0x80000000 0x00 0x20000000>` |
| `/iram@200000` (L752) | `compatible="zte,iram"`<br>`status="disabled"` | `reg=<0x00 0x200000 0x00 0x100000>` |
| `/soc` (L758) | `compatible="simple-bus"` | `ranges=true` |
| `/soc/boot_ctrl@10e10170` (L764) | `compatible="zxic,boot-ctrl", "syscon"` | `reg=<0x00 0x10e10170 0x00 0x04>` |
| `/soc/toprst@10e10060` (L769) | `compatible="zte,zx296718-reset"` | `reg=<0x00 0x10e10060 0x00 0x04>` |
| `/soc/localrst@10e10070` (L776) | `compatible="zte,zx296718-reset"` | `reg=<0x00 0x10e10070 0x00 0x08>` |
| `/soc/topcrm@10e10000` (L782) | `compatible="zte,zx279133-topcrm", "syscon"` | `reg=<0x00 0x10e10000 0x00 0x10000>` |
| `/soc/lsp_crm@10d00000` (L788) | `compatible="zxic,lsp-clock", "syscon"` | `reg=<0x00 0x10d00000 0x00 0x100>` |
| `/soc/lsp_crm@14f00000` (L794) | `compatible="zte,lsp1_crm", "zxic,lsp-clock", "syscon"` | `reg=<0x00 0x14f00000 0x00 0x100>` |
| `/soc/sys_ctrl` (L800) | `compatible="zte,zx279133-sysctrl", "syscon"` | `reg=<0x00 0x10e00000 0x00 0x10000>` |
| `/soc/rstctrl` (L805) | `compatible="zxic,zx-rstctrl", "syscon"` | `reg=<0x00 0x10e10208 0x00 0x04>` |
| `/soc/reboot@10e10204` (L811) | `compatible="zxic,zx-reboot"` | `reg=<0x00 0x10e10204 0x00 0x04>` |
| `/soc/pinctrl@10e20000` (L818) | `compatible="zte,zx-pinctrl"` | `reg=<0x00 0x10e20000 0x00 0x1000>` |
| `/soc/pinctrl@10e20000/i2c0-pins` (L844) | — | `function="i2c0"`<br>`pins="p0-6", "p0-7"` |
| `/soc/pinctrl@10e20000/i2c1-pins` (L850) | — | `function="i2c1"`<br>`pins="p0-8", "p0-9"` |
| `/soc/pinctrl@10e20000/i2s0-pins` (L856) | — | `function="i2s0"`<br>`pins="p2-10", "p2-11", "p3-6", "p3-7", "p3-8"` |
| `/soc/pinctrl@10e20000/i2s1-pins` (L863) | — | `function="i2s1"`<br>`pins="p0-1", "p0-2", "p0-3", "p0-4", "p0-5"` |
| `/soc/pinctrl@10e20000/i2s2-pins` (L870) | — | `function="i2s2"`<br>`pins="p3-0", "p3-1", "p3-2", "p3-3", "p3-4", "p3-5"` |
| `/soc/pinctrl@10e20000/pwm_pins` (L877) | — | `function="pwm"`<br>`pins="p0-8", "p0-9", "p0-12", "p0-13"` |
| `/soc/pinctrl@10e20000/mdio0-pins` (L884) | — | `function="mdio0"`<br>`pins="p2-10", "p2-11"` |
| `/soc/pinctrl@10e20000/mdio1-pins` (L890) | — | `function="mdio1"`<br>`pins="p2-0", "p2-1"` |
| `/soc/pinctrl@10e20000/mmc-pins` (L896) | — | `function="mmc"`<br>`pins="p3-0", "p3-1", "p3-2", "p3-3", "p3-4", "p3-5", "p3-6", "p3-7"` |
| `/soc/pinctrl@10e20000/nand_pins` (L903) | — | `function="nand"`<br>`pins="p3-0", "p3-1", "p3-2", "p3-3", "p3-4", "p3-5", "p3-6", "p3-7", "p3-8", "p3-9", "p3-10", "p3-11", "p3-12", "p4-0", "p4-1"` |
| `/soc/pinctrl@10e20000/spifc-pins` (L910) | — | `function="spifc"`<br>`pins="p3-9", "p3-10", "p3-11", "p3-12", "p4-0", "p4-1"` |
| `/soc/pinctrl@10e20000/pcie0-pins` (L917) | — | `function="pcie0"`<br>`pins="p4-2", "p4-3"` |
| `/soc/pinctrl@10e20000/pcie1-pins` (L924) | — | `function="pcie1"`<br>`pins="p4-4", "p4-5"` |
| `/soc/pinctrl@10e20000/lan_led0_pins` (L931) | — | `function="lan_led0"`<br>`pins="p4-6"` |
| `/soc/pinctrl@10e20000/lan_led1_pins` (L937) | — | `function="lan_led1"`<br>`pins="p4-7"` |
| `/soc/pinctrl@10e20000/lan_led2_pins` (L943) | — | `function="lan_led2"`<br>`pins="p4-8"` |
| `/soc/pinctrl@10e20000/lan_led3_pins` (L949) | — | `function="lan_led3"`<br>`pins="p4-9"` |
| `/soc/pinctrl@10e20000/pon_plat_pins` (L955) | — | `function="pon_ben"`<br>`pins="p2-2"` |
| `/soc/pinctrl@10e20000/ssp0-pins` (L961) | — | `function="ssp0"`<br>`pins="p1-0", "p1-1", "p1-2", "p1-3", "p1-4"` |
| `/soc/pinctrl@10e20000/ssp1-pins` (L968) | — | `function="ssp1"`<br>`pins="p1-5", "p1-6", "p1-7", "p1-8", "p1-9"` |
| `/soc/pinctrl@10e20000/zsi_pins` (L975) | — | `function="zsi"`<br>`pins="p5-0", "p5-1", "p5-2", "p5-3"` |
| `/soc/pinctrl@10e20000/zsi1_pins` (L982) | — | `function="zsi1"`<br>`pins="p1-0", "p1-1", "p1-2", "p1-3"` |
| `/soc/pinctrl@10e20000/isi_pins` (L989) | — | `function="isi"`<br>`pins="p5-0", "p5-2", "p5-3"` |
| `/soc/pinctrl@10e20000/isi1_pins` (L996) | — | `function="isi1"`<br>`pins="p1-0", "p1-2", "p1-3"` |
| `/soc/pinctrl@10e20000/pcm_pins` (L1003) | — | `function="pcm"`<br>`pins="p5-0", "p5-1", "p5-2", "p5-3"` |
| `/soc/pinctrl@10e20000/pcm1_pins` (L1010) | — | `function="pcm1"`<br>`pins="p0-1", "p0-2", "p0-3", "p0-4"` |
| `/soc/pvt@10e70000` (L1018) | `compatible="zxic,zx-pvt-sensor-cln22ulp"`<br>`status="okay"` | `reg=<0x00 0x10e70000 0x00 0x10000>`<br>`interrupts=<0x00 0xbe 0x04 0x00 0xbf 0x04 0x00 0xc0 0x04 0x00 0xc1 0x04>`<br>`clocks=<0x0f 0x00>` |
| `/soc/bdinfo@0` (L1030) | `compatible="zxic,zx279133-board", "zxic,zx-board"` | — |
| `/soc/gpio@10d10000` (L1035) | `compatible="zte,zx-gpio"` | `reg=<0x00 0x10d10000 0x00 0x40>`<br>`interrupts=<0x00 0x22 0x04>` |
| `/soc/gpio@10d10040` (L1044) | `compatible="zte,zx-gpio"` | `reg=<0x00 0x10d10040 0x00 0x40>`<br>`interrupts=<0x00 0x23 0x04>` |
| `/soc/gpio@10d10080` (L1053) | `compatible="zte,zx-gpio"` | `reg=<0x00 0x10d10080 0x00 0x40>`<br>`interrupts=<0x00 0x24 0x04>` |
| `/soc/gpio@10d100c0` (L1062) | `compatible="zte,zx-gpio"` | `reg=<0x00 0x10d100c0 0x00 0x40>`<br>`interrupts=<0x00 0x25 0x04>` |
| `/soc/gpio@10d10100` (L1071) | `compatible="zte,zx-gpio"` | `reg=<0x00 0x10d10100 0x00 0x40>`<br>`interrupts=<0x00 0x26 0x04>` |
| `/soc/serial@10d0d000` (L1080) | `compatible="arm,sbsa-uart", "arm,pl011", "arm,primecell"`<br>`status="okay"` | `reg=<0x00 0x10d0d000 0x00 0x1000>`<br>`interrupts=<0x00 0x1f 0x04>`<br>`clocks=<0x11 0x01 0x11 0x00>`<br>`clock-names="uartclk", "apb_pclk"` |
| `/soc/serial@10d0e000` (L1094) | `compatible="arm,sbsa-uart", "arm,pl011", "arm,primecell"`<br>`status="disabled"` | `reg=<0x00 0x10d0e000 0x00 0x1000>`<br>`interrupts=<0x00 0x20 0x04>`<br>`clocks=<0x12 0x01 0x12 0x00>`<br>`clock-names="uartclk", "apb_pclk"` |
| `/soc/serial@10d11000` (L1106) | `compatible="arm,sbsa-uart", "arm,pl011", "arm,primecell"`<br>`status="disabled"` | `reg=<0x00 0x10d11000 0x00 0x1000>`<br>`interrupts=<0x00 0x27 0x04>`<br>`clocks=<0x13 0x01 0x13 0x00>`<br>`clock-names="uartclk", "apb_pclk"` |
| `/soc/i2c@14f03000` (L1118) | `compatible="zte,zx-i2c"`<br>`status="okay"` | `reg=<0x00 0x14f03000 0x00 0x1000>`<br>`interrupts=<0x00 0x2b 0x04>`<br>`clocks=<0x14 0x01>`<br>`clock-names="i2cclk"`<br>`resets=<0x14 0x00>`<br>`clock-frequency=<0x186a0>` |
| `/soc/i2c@14f04000` (L1129) | `compatible="zte,zx-i2c"`<br>`status="okay"` | `reg=<0x00 0x14f04000 0x00 0x1000>`<br>`interrupts=<0x00 0x2c 0x04>`<br>`clocks=<0x15 0x01>`<br>`clock-names="i2cclk"`<br>`resets=<0x15 0x00>`<br>`clock-frequency=<0x186a0>` |
| `/soc/wdt@14f09000` (L1140) | `compatible="zte,lsp1-watchdog"`<br>`status="okay"` | `reg=<0x00 0x14f09000 0x00 0x1000>`<br>`interrupts=<0x00 0x35 0x04>`<br>`clocks=<0x16 0x01>`<br>`clock-names="wdtclk"` |
| `/soc/wdt@14f0a000` (L1150) | `compatible="zte,lsp1-watchdog"`<br>`status="okay"` | `reg=<0x00 0x14f0a000 0x00 0x1000>`<br>`interrupts=<0x00 0x36 0x04>`<br>`clocks=<0x18 0x01>`<br>`clock-names="wdtclk"` |
| `/soc/i2s@14f0d000` (L1160) | `compatible="zte,zx-i2s"`<br>`status="disabled"` | `reg=<0x00 0x14f0d000 0x00 0x1000>`<br>`interrupts=<0x00 0x39 0x04>`<br>`clocks=<0x19 0x01 0x19 0x00>`<br>`clock-names="i2s_wclk", "i2s_pclk"`<br>`pinctrl-0=<0x1a>` |
| `/soc/i2s@14f0e000` (L1171) | `compatible="zte,zx-i2s"`<br>`status="disabled"` | `reg=<0x00 0x14f0e000 0x00 0x1000>`<br>`interrupts=<0x00 0x3a 0x04>`<br>`clocks=<0x1b 0x01 0x1b 0x00>`<br>`clock-names="i2s_wclk", "i2s_pclk"`<br>`pinctrl-0=<0x1c>` |
| `/soc/i2s@14f0f000` (L1182) | `compatible="zte,zx-i2s"`<br>`status="disabled"` | `reg=<0x00 0x14f0f000 0x00 0x1000>`<br>`interrupts=<0x00 0x3b 0x04>`<br>`clocks=<0x1d 0x01 0x1d 0x00>`<br>`clock-names="i2s_wclk", "i2s_pclk"`<br>`pinctrl-0=<0x1e>` |
| `/soc/pwm@14f10000` (L1193) | `compatible="zte,zx-pwm"`<br>`status="disabled"` | `reg=<0x00 0x14f10000 0x00 0x1000>`<br>`clocks=<0x1f 0x01 0x1f 0x00>`<br>`clock-names="pwm_wclk", "pwm_pclk"`<br>`pinctrl-0=<0x20>` |
| `/soc/regulator@cpu` (L1205) | `compatible="pwm-regulator"` | `pwms=<0x21 0x01 0x640 0x00>`<br>`regulator-min-microvolt="", "\f5"`<br>`regulator-max-microvolt=<0xd7168>` |
| `/soc/efuse@14f11000` (L1217) | `compatible="zte,efuse", "zxic,zx-efuse", "syscon"` | `reg=<0x00 0x14f11000 0x00 0x1000>`<br>`clocks=<0x22 0x01 0x22 0x00>`<br>`clock-names="efuse_wclk", "efuse_pclk"` |
| `/soc/usb3@15008000` (L1224) | `compatible="generic-xhci"`<br>`status="disabled"` | `reg=<0x00 0x15008000 0x00 0x4000>`<br>`interrupts=<0x00 0x3f 0x04>`<br>`clocks=<0x23 0x06 0x23 0x04>`<br>`clock-names="core", "reg"` |
| `/soc/usb3@15010000` (L1234) | `compatible="generic-xhci"`<br>`status="okay"` | `reg=<0x00 0x15010000 0x00 0x4000>`<br>`interrupts=<0x00 0x41 0x04>`<br>`clocks=<0x23 0x06 0x23 0x04>`<br>`clock-names="core", "reg"` |
| `/soc/mdio@14f01000` (L1244) | `compatible="zte,zx-mdio"`<br>`status="okay"` | `reg=<0x00 0x14f01000 0x00 0x1000>`<br>`clocks=<0x24 0x01>`<br>`resets=<0x24 0x00>` |
| `/soc/mdio@14f02000` (L1255) | `compatible="zte,zx-mdio"`<br>`status="okay"` | `reg=<0x00 0x14f02000 0x00 0x1000>`<br>`clocks=<0x25 0x01>`<br>`resets=<0x25 0x00>` |
| `/soc/dwmmc0@0x08a00000` (L1266) | `compatible="zte,zx296718-dw-mshc"`<br>`status="disabled"` | `reg=<0x00 0x8a00000 0x00 0x1000>`<br>`interrupts=<0x00 0x07 0x04>`<br>`clocks=<0x26 0x09 0x26 0x08 0x26 0x0a>`<br>`clock-names="ciu", "biu", "fb_sdmmc_cdet_clk"`<br>`resets=<0x27 0x02>`<br>`pinctrl-0=<0x28>`<br>`clock-frequency=<0x17d7840>` |
| `/soc/nand@01000000` (L1287) | `compatible="zxic,zx-denali-nand", "denali,denali-nand-dt"`<br>`status="disabled"` | `reg=<0x00 0x1000000 0x00 0x80000 0x00 0x1080000 0x00 0x80000>`<br>`reg-names="denali_reg", "nand_data"`<br>`interrupts=<0x00 0x00 0x04>`<br>`clocks=<0x2a 0x26 0x05 0x26 0x05>`<br>`clock-names="nand", "nand_x", "ecc"`<br>`resets=<0x27 0x01>`<br>`reset-names="top-reset"`<br>`pinctrl-0=<0x29>` |
| `/soc/nand@01000000/partitions` (L1304) | `compatible="dynamic-partitions"` | — |
| `/soc/spifc@0x10d0f000` (L1311) | `compatible="zxic,zx-spifc"`<br>`status="disabled"` | `reg=<0x00 0x10d0f000 0x00 0x40000>`<br>`interrupts=<0x00 0x21 0x04>`<br>`clocks=<0x2d 0x01>`<br>`clock-names="wclk"`<br>`resets=<0x2d 0x00>`<br>`pinctrl-0=<0x2e>` |
| `/soc/spifc@0x10d0f000/spi-flash@0` (L1327) | `compatible="spi-nand"` | `reg=<0x00>` |
| `/soc/spifc@0x10d0f000/spi-flash@0/partitions` (L1336) | `compatible="dynamic-partitions"` | — |
| `/soc/wifi0@f00000` (L1344) | `compatible="qcom,cnss-qcn9224"`<br>`status="ok"` | — |
| `/soc/wifi1@f00000` (L1356) | `compatible="qcom,cnss-qcn9224"`<br>`status="ok"` | — |
| `/soc/msi@1f000000` (L1368) | `compatible="zte,msi-1.0-zxic"`<br>`status="disabled"` | `reg=<0x00 0x1f000000 0x00 0x400000 0x00 0x15204000 0x00 0x1000 0x00 0x10e00000 0x00 0x04>`<br>`reg-names="Dbi", "Phy", "vector_slave"` |
| `/soc/pcie@15200000` (L1380) | `compatible="zte,zx279133-pcie"`<br>`status="disabled"` | `reg=<0x00 0x15200000 0x00 0x2000 0x00 0x15204000 0x00 0x1000 0x00 0x1f000000 0x00 0x400000 0x00 0x2f800000 0x00 0x800000 0x00 0x10e00000 0x00 0x04 0x00 0x10e00000 0x00 0x10000 0x00 0x10e10000 0x00 0x10000 0x00 0x600060 0x00 0x04>`<br>`reg-names="Csr", "Phy", "Dbi", "Config", "Msi", "Sys", "Top", "Spimode"`<br>`interrupts=<0x00 0x69 0x08>`<br>`ranges=<0x1000000 0x00 0x2f000000 0x00 0x2f000000 0x00 0x100000 0x2000000 0x00 0x20000000 0x00 0x20000000 0x00 0xf000000>`<br>`dma-ranges=<0x42000000 0x00 0x80000000 0x00 0x80000000 0x00 0x40000000>`<br>`pinctrl-0=<0x30>`<br>`pinctrl-1=<0x31>` |
| `/soc/pcie@15200000/pcie0_rp` (L1406) | `status="ok"` | `reg=<0x00 0x00 0x00 0x00 0x00>` |
| `/soc/pcie@15200000/pcie0_rp/qcom,mhi@0` (L1410) | — | `reg=<0x00 0x00 0x00 0x00 0x00>` |
| `/soc/msi@1f400000` (L1417) | `compatible="zte,msi-1.0-zxic"`<br>`status="disabled"` | `reg=<0x00 0x1f400000 0x00 0x400000 0x00 0x15204000 0x00 0x1000 0x00 0x10e00004 0x00 0x04>`<br>`reg-names="Dbi", "Phy", "vector_slave"` |
| `/soc/pcie@15202000` (L1429) | `compatible="zte,zx279133-pcie"`<br>`status="disabled"` | `reg=<0x00 0x15202000 0x00 0x2000 0x00 0x15204000 0x00 0x1000 0x00 0x1f400000 0x00 0x400000 0x00 0x3f800000 0x00 0x800000 0x00 0x10e00004 0x00 0x04 0x00 0x10e00000 0x00 0x10000 0x00 0x10e10000 0x00 0x10000 0x00 0x60006c 0x00 0x04>`<br>`reg-names="Csr", "Phy", "Dbi", "Config", "Msi", "Sys", "Top", "Spimode"`<br>`interrupts=<0x00 0x9c 0x08>`<br>`ranges=<0x1000000 0x00 0x3f000000 0x00 0x3f000000 0x00 0x100000 0x2000000 0x00 0x30000000 0x00 0x30000000 0x00 0xf000000>`<br>`dma-ranges=<0x42000000 0x00 0x80000000 0x00 0x80000000 0x00 0x40000000>`<br>`pinctrl-0=<0x30>`<br>`pinctrl-1=<0x31>` |
| `/soc/pcie@15202000/pcie1_rp` (L1455) | `status="ok"` | `reg=<0x00 0x00 0x00 0x00 0x00>` |
| `/soc/pcie@15202000/pcie1_rp/qcom,mhi@1` (L1459) | — | `reg=<0x00 0x00 0x00 0x00 0x00>` |
| `/soc/ssp@0x14f05000` (L1466) | `compatible="zte,zx27913x-ssp"`<br>`status="disabled"` | `reg=<0x00 0x14f05000 0x00 0x1000>`<br>`interrupts=<0x00 0x2d 0x04>`<br>`clocks=<0x33 0x01>`<br>`resets=<0x33 0x00>`<br>`pinctrl-0=<0x34>` |
| `/soc/ssp@0x14f05000/silicon@0` (L1481) | `compatible="rohm,dh2228fv"` | `reg=<0x00>` |
| `/soc/ssp@0x14f06000` (L1488) | `compatible="zte,zx27913x-ssp"`<br>`status="disabled"` | `reg=<0x00 0x14f06000 0x00 0x1000>`<br>`interrupts=<0x00 0x2e 0x04>`<br>`clocks=<0x35 0x01>`<br>`resets=<0x35 0x00>`<br>`pinctrl-0=<0x36>` |
| `/soc/ssp@0x14f06000/silicon@1` (L1503) | `compatible="rohm,dh2228fv"` | `reg=<0x00>` |
| `/soc/tdm@0x14f07000` (L1510) | `compatible="zte,zx27913x-tdm"`<br>`status="disabled"` | `reg=<0x00 0x14f07000 0x00 0x1000>`<br>`interrupts=<0x00 0x2f 0x04>`<br>`clocks=<0x37 0x01 0x38 0x01 0x39 0x01>`<br>`clock-names="tdmclk", "zsiclk", "isiclk"`<br>`resets=<0x37 0x00 0x37 0x01>`<br>`pinctrl-0=<0x3a>`<br>`pinctrl-1=<0x3b>`<br>`pinctrl-2=<0x3c>` |
| `/soc/tdm@0x14f08000` (L1526) | `compatible="zte,zx27913x-tdm"`<br>`status="disabled"` | `reg=<0x00 0x14f08000 0x00 0x1000>`<br>`interrupts=<0x00 0x30 0x04>`<br>`clocks=<0x3d 0x01 0x3e 0x01 0x3f 0x01>`<br>`clock-names="tdmclk", "zsiclk", "isiclk"`<br>`resets=<0x3d 0x00 0x3d 0x01>`<br>`pinctrl-0=<0x40>`<br>`pinctrl-1=<0x41>`<br>`pinctrl-2=<0x42>` |
| `/soc/pon_plat` (L1542) | `compatible="zxic,pon-plat"` | — |
| `/soc/leds` (L1546) | `compatible="zxic,zx-leds"` | — |
| `/soc/keys` (L1550) | `compatible="zxic,gpio-keys"` | — |
| `/soc/key_maps` (L1554) | `compatible="zxic,gpio-key-evdev"` | — |
| `/soc/pon@17000000` (L1558) | `compatible="zte,zx279133-pon"`<br>`status="okay"` | `reg=<0x00 0x17000000 0x00 0x1000000 0x00 0x10e00000 0x00 0x1000 0x00 0x10e10000 0x00 0x10000 0x00 0x10e20000 0x00 0x1000 0x00 0x14f11000 0x00 0x1000>`<br>`interrupts=<0x00 0x06 0x04>` |
| `/soc/pps@18000000` (L1565) | `compatible="zte,zx279133-pps"`<br>`status="disabled"` | `reg=<0x00 0x18000000 0x00 0x1000000>`<br>`interrupts=<0x00 0x3d 0x04 0x00 0x3e 0x04>` |
| `/soc/nppt@19000000` (L1572) | `compatible="zte,zx279133-nppt"`<br>`status="disabled"` | `reg=<0x00 0x19000000 0x00 0x1000000>`<br>`interrupts=<0x00 0x05 0x04>` |
| `/soc/rgmii@15900000` (L1579) | `compatible="zte,zx279133-rgmii"`<br>`status="disabled"` | `reg=<0x00 0x15900000 0x00 0x100000>` |
| `/soc/pon_serdes@16000000` (L1585) | `compatible="zte,zx279133-pon_serdes"`<br>`status="disabled"` | `reg=<0x00 0x16000000 0x00 0x10000>` |
| `/soc/pon_serdes_pll@16010000` (L1591) | `compatible="zte,zx279133-pon_serdes_pll"`<br>`status="disabled"` | `reg=<0x00 0x16010000 0x00 0x10000>` |
| `/soc/uni_serdes@16100000` (L1597) | `compatible="zte,zx279133-uni_serdes"`<br>`status="disabled"` | `reg=<0x00 0x16100000 0x00 0x100000>` |
| `/soc/pcu@10e30000` (L1603) | `compatible="zte,zx279133-pcu"`<br>`status="disabled"` | `reg=<0x00 0x10e30000 0x00 0x10000>` |
| `/soc/gephy_apb@15400000` (L1609) | `compatible="zte,zx279133-gephy-apb"`<br>`status="disabled"` | `reg=<0x00 0x15400000 0x00 0x500000>` |
| `/soc/ipsec@12000000` (L1615) | `compatible="zte,zx279133-ipsec"`<br>`status="disabled"` | `reg=<0x00 0x12000000 0x00 0x1000000 0x00 0x13000000 0x00 0x1000000>`<br>`interrupts=<0x00 0xae 0x04 0x00 0xaf 0x04 0x00 0xb0 0x04 0x00 0xb1 0x04 0x00 0xb2 0x04 0x00 0xb3 0x04 0x00 0xb4 0x04 0x00 0xb5 0x04 0x00 0xb6 0x04 0x00 0xb7 0x04 0x00 0xb8 0x04>` |
| `/soc/idm@19280000` (L1624) | `compatible="zte,zx279133-idm-intr"`<br>`status="disabled"` | `interrupts=<0x00 0x01 0x04 0x00 0x02 0x04 0x00 0x03 0x04 0x00 0x04 0x04>` |
| `/soc/xpcs@1a000000` (L1630) | `compatible="zte,zx279133-xmac0-pcs"`<br>`status="disabled"` | `reg=<0x00 0x1a000000 0x00 0x2000000>` |
| `/soc/se@18040000` (L1636) | `compatible="zte,zx279133-se-ddr"`<br>`status="okay"` | — |
| `/soc/woe@19240000` (L1647) | `compatible="zte,zx279133-woe-ddr"`<br>`status="okay"` | `interrupts=<0x00 0xac 0x04 0x00 0xad 0x04>` |
| `/soc/bmu@1903c000` (L1653) | `compatible="zte,zx279133-bmu-ddr"`<br>`status="okay"` | — |
| `/part@256/partition@whole` (L1673) | — | `reg=<0x00 0x10000000>`<br>`label="whole flash"` |
| `/part@256/partition@bootloader` (L1678) | — | `reg=<0x00 0x100000>`<br>`label="bootloader"` |
| `/part@256/partition@tag` (L1683) | — | `reg=<0x100000 0x100000>`<br>`label="tags"` |
| `/part@256/partition@usercfg` (L1688) | — | `reg=<0x200000 0x200000>`<br>`label="usercfg"` |
| `/part@256/partition@defcfg` (L1693) | — | `reg=<0x400000 0x200000>`<br>`label="defcfg"` |
| `/part@256/partition@kern0` (L1698) | — | `reg=<0x600000 0x2900000>`<br>`label="kernel1"` |
| `/part@256/partition@kern1` (L1703) | — | `reg=<0x2f00000 0x2900000>`<br>`label="kernel2"` |
| `/part@256/partition@rootfs0` (L1708) | — | `reg=<0x600000 0x2900000>`<br>`label="rootfs1"` |
| `/part@256/partition@rootfs1` (L1713) | — | `reg=<0x2f00000 0x2900000>`<br>`label="rootfs2"` |
| `/part@256/partition@Plugin` (L1718) | — | `reg=<0x5800000 0x2800000>`<br>`label="Plugin"` |
| `/part@128/partition@whole` (L1729) | — | `reg=<0x00 0x8000000>`<br>`label="whole flash"` |
| `/part@128/partition@bootloader` (L1734) | — | `reg=<0x00 0x100000>`<br>`label="bootloader"` |
| `/part@128/partition@tag` (L1739) | — | `reg=<0x100000 0x100000>`<br>`label="tags"` |
| `/part@128/partition@usercfg` (L1744) | — | `reg=<0x200000 0x200000>`<br>`label="usercfg"` |
| `/part@128/partition@defcfg` (L1749) | — | `reg=<0x400000 0x200000>`<br>`label="defcfg"` |
| `/part@128/partition@kern0` (L1754) | — | `reg=<0x600000 0x2900000>`<br>`label="kernel1"` |
| `/part@128/partition@kern1` (L1759) | — | `reg=<0x2f00000 0x2900000>`<br>`label="kernel2"` |
| `/part@128/partition@rootfs0` (L1764) | — | `reg=<0x600000 0x2900000>`<br>`label="rootfs1"` |
| `/part@128/partition@rootfs1` (L1769) | — | `reg=<0x2f00000 0x2900000>`<br>`label="rootfs2"` |
| `/part@128/partition@Plugin` (L1774) | — | `reg=<0x5800000 0x2800000>`<br>`label="Plugin"` |

## Mainline SoC normalized resource inventory

The board status overrides and wiring follow in the next table.
| Node (source line) | compatible / status | resources & wiring |
|---|---|---|
| `/` (L9) | `compatible="zte,zx279133"` | — |
| `/oscillator-25m` (L15) | `compatible="fixed-clock"` | `clock-frequency=<25000000>` |
| `/clock-pll-lsp-2000m` (L22) | `compatible="fixed-clock"` | `clock-frequency=<2000000000>` |
| `/clock-pll-1376m256` (L29) | `compatible="fixed-clock"` | `clock-frequency=<1376256000>` |
| `/clock-pll-fpp-2500m` (L36) | `compatible="fixed-clock"` | `clock-frequency=<2500000000>` |
| `/clock-pll-cpu-2000m` (L44) | `compatible="fixed-clock"` | `clock-frequency=<2000000000>` |
| `/cpus/cpu@0` (L55) | `compatible="arm,cortex-a53"` | `reg=<0x0 0x0>`<br>`clocks=<&topcrm ZX279133_TOPCRM_CLK_A53_MCLK>` |
| `/cpus/cpu@1` (L63) | `compatible="arm,cortex-a53"` | `reg=<0x0 0x1>`<br>`clocks=<&topcrm ZX279133_TOPCRM_CLK_A53_MCLK>` |
| `/psci` (L72) | `compatible="arm,psci-0.2"` | — |
| `/interrupt-controller@500000` (L77) | `compatible="arm,gic-v3"` | `reg=<0x0 0x00500000 0x0 0x10000>, <0x0 0x00540000 0x0 0x80000>`<br>`interrupts=<GIC_PPI 9 IRQ_TYPE_LEVEL_HIGH>` |
| `/timer` (L88) | `compatible="arm,armv8-timer"` | `interrupts=<GIC_PPI 13 (GIC_CPU_MASK_SIMPLE(2) \| IRQ_TYPE_LEVEL_LOW)>, <GIC_PPI 14 (GIC_CPU_MASK_SIMPLE(2) \| IRQ_TYPE_LEVEL_LOW)>, <GIC_PPI 11 (GIC_CPU_MASK_SIMPLE(2) \| IRQ_TYPE_LEVEL_LOW)>, <GIC_PPI 10 (GIC_CPU_MASK_SIMPLE(2) \| IRQ_TYPE_LEVEL_LOW)>` |
| `/soc` (L100) | `compatible="simple-bus"` | `ranges=true` |
| `/soc/clock-controller@10e10000` (L106) | `compatible="zte,zx279133-topcrm", "syscon"` | `reg=<0x0 0x10e10000 0x0 0x60>`<br>`clocks=<&osc25m>, <&pll_lsp>, <&pll_1376m>, <&pll_fpp>, <&pll_cpu>`<br>`clock-names="osc25m", "pll-lsp", "pll-1376m", "pll-fpp", "pll-cpu"` |
| `/soc/syscon@10e00078` (L116) | `compatible="zte,zx279133-idm-cci", "syscon"` | `reg=<0x0 0x10e00078 0x0 0x8>` |
| `/soc/reset-controller@10e10060` (L121) | `compatible="zte,zx296718-reset"` | `reg=<0x0 0x10e10060 0x0 0x4>` |
| `/soc/reset-controller@10e10070` (L127) | `compatible="zte,zx296718-reset"` | `reg=<0x0 0x10e10070 0x0 0x8>` |
| `/soc/syscon@10e10208` (L133) | `compatible="zte,zx279133-wdt-reset", "syscon"` | `reg=<0x0 0x10e10208 0x0 0x4>` |
| `/soc/clock-controller@10d00000` (L138) | `compatible="zte,zx279133-lsp-clk", "syscon"` | `reg=<0x0 0x10d00000 0x0 0x100>`<br>`clocks=<&topcrm ZX279133_TOPCRM_CLK_LSP0_PCLK>, <&topcrm ZX279133_TOPCRM_CLK_LSP0_25M>, <&topcrm ZX279133_TOPCRM_CLK_LSP0_100M>, <&topcrm ZX279133_TOPCRM_CLK_SPIFC_WCLK>`<br>`clock-names="pclk", "wclk25", "wclk100", "spifc_wclk"` |
| `/soc/serial@10d0d000` (L150) | `compatible="zte,zx279133-uart", "arm,pl011", "arm,primecell"`<br>`status="disabled"` | `reg=<0x0 0x10d0d000 0x0 0x1000>`<br>`interrupts=<GIC_SPI 31 IRQ_TYPE_LEVEL_HIGH>`<br>`clocks=<&lsp0_clk ZX279133_LSP_CLK_UART0_WCLK>, <&lsp0_clk ZX279133_LSP_CLK_UART0_PCLK>`<br>`clock-names="uartclk", "apb_pclk"`<br>`resets=<&lsp0_clk ZX279133_LSP_RESET_UART0>` |
| `/soc/temperature-sensor@10e70000` (L167) | `compatible="zte,zx279133-pvt"`<br>`status="disabled"` | `reg=<0x0 0x10e70000 0x0 0x10000>`<br>`clocks=<&topcrm ZX279133_TOPCRM_CLK_PVT_PCLK>, <&topcrm ZX279133_TOPCRM_CLK_TEMPSENSOR_WCLK>`<br>`clock-names="pclk", "wclk"` |
| `/soc/pinctrl@10e20000` (L179) | `compatible="zte,zx279133-pinctrl"` | `reg=<0x0 0x10e20000 0x0 0x1000>` |
| `/soc/pinctrl@10e20000/spifc-pins` (L183) | — | `function="spifc"`<br>`pins="p3-9", "p3-10", "p3-11", "p3-12", "p4-0", "p4-1"` |
| `/soc/gpio@10d10000` (L190) | `compatible="zte,zx279133-gpio"` | `reg=<0x0 0x10d10000 0x0 0x40>`<br>`interrupts=<GIC_SPI 34 IRQ_TYPE_LEVEL_HIGH>` |
| `/soc/gpio@10d10040` (L203) | `compatible="zte,zx279133-gpio"` | `reg=<0x0 0x10d10040 0x0 0x40>`<br>`interrupts=<GIC_SPI 35 IRQ_TYPE_LEVEL_HIGH>` |
| `/soc/gpio@10d10080` (L216) | `compatible="zte,zx279133-gpio"` | `reg=<0x0 0x10d10080 0x0 0x40>`<br>`interrupts=<GIC_SPI 36 IRQ_TYPE_LEVEL_HIGH>` |
| `/soc/gpio@10d100c0` (L228) | `compatible="zte,zx279133-gpio"` | `reg=<0x0 0x10d100c0 0x0 0x40>`<br>`interrupts=<GIC_SPI 37 IRQ_TYPE_LEVEL_HIGH>` |
| `/soc/gpio@10d10100` (L240) | `compatible="zte,zx279133-gpio"` | `reg=<0x0 0x10d10100 0x0 0x40>`<br>`interrupts=<GIC_SPI 38 IRQ_TYPE_LEVEL_HIGH>` |
| `/soc/spi@10d0f000` (L252) | `compatible="zte,zx279133-sfc"`<br>`status="disabled"` | `reg=<0x0 0x10d0f000 0x0 0x1000>`<br>`interrupts=<GIC_SPI 33 IRQ_TYPE_LEVEL_HIGH>`<br>`clocks=<&lsp0_clk ZX279133_LSP_CLK_SPIFC_PCLK>, <&lsp0_clk ZX279133_LSP_CLK_SPIFC_WCLK>`<br>`clock-names="pclk", "wclk"`<br>`resets=<&lsp0_clk ZX279133_LSP_RESET_SPIFC>`<br>`pinctrl-0=<&spifc_pins>` |
| `/soc/spi@10d0f000/flash@0` (L268) | `compatible="spi-nand"` | `reg=<0>` |
| `/soc/spi@10d0f000/flash@0/partitions` (L273) | `compatible="fixed-partitions"` | — |
| `/soc/spi@10d0f000/flash@0/partitions/partition@0` (L278) | — | `reg=<0x000000 0x100000>`<br>`label="bootloader"` |
| `/soc/spi@10d0f000/flash@0/partitions/partition@100000` (L284) | — | `reg=<0x100000 0x100000>`<br>`label="tags"` |
| `/soc/spi@10d0f000/flash@0/partitions/partition@200000` (L289) | — | `reg=<0x200000 0x200000>`<br>`label="usercfg"` |
| `/soc/spi@10d0f000/flash@0/partitions/partition@400000` (L295) | — | `reg=<0x400000 0x200000>`<br>`label="defcfg"` |
| `/soc/spi@10d0f000/flash@0/partitions/partition@600000` (L301) | — | `reg=<0x600000 0x2900000>`<br>`label="firmware-slot1"` |
| `/soc/spi@10d0f000/flash@0/partitions/partition@2f00000` (L307) | — | `reg=<0x2f00000 0x2900000>`<br>`label="firmware-slot2"` |
| `/soc/spi@10d0f000/flash@0/partitions/partition@5800000` (L313) | — | `reg=<0x5800000 0x2800000>`<br>`label="Plugin"` |
| `/soc/usb3@15010000` (L322) | `compatible="zte,zx279133-xhci"`<br>`status="disabled"` | `reg=<0x0 0x15010000 0x0 0x4000>`<br>`interrupts=<GIC_SPI 65 IRQ_TYPE_LEVEL_HIGH>`<br>`clocks=<&topcrm ZX279133_TOPCRM_CLK_USB_ACLK>, <&topcrm ZX279133_TOPCRM_CLK_USB_PCLK>, <&topcrm ZX279133_TOPCRM_CLK_USB_CCI_ACLK>`<br>`clock-names="core", "reg", "cci"` |
| `/soc/clock-controller@14f00000` (L334) | `compatible="zte,zx279133-lsp1-clk"` | `reg=<0x0 0x14f00000 0x0 0x100>`<br>`clocks=<&topcrm ZX279133_TOPCRM_CLK_LSP1_PCLK>, <&topcrm ZX279133_TOPCRM_CLK_LSP1_32K>, <&topcrm ZX279133_TOPCRM_CLK_LSP1_25M>, <&topcrm ZX279133_TOPCRM_CLK_LSP1_100M>`<br>`clock-names="pclk", "wclk32k", "wclk25m", "wclk100m"` |
| `/soc/mdio@14f01000` (L346) | `compatible="zte,zx279133-mdio"`<br>`status="disabled"` | `reg=<0x0 0x14f01000 0x0 0x1000>`<br>`clocks=<&lsp1_clk ZX279133_LSP1_CLK_MDIO0_PCLK>, <&lsp1_clk ZX279133_LSP1_CLK_MDIO0_WCLK>`<br>`clock-names="pclk", "wclk"`<br>`resets=<&lsp1_clk ZX279133_LSP1_RESET_MDIO0>` |
| `/soc/mdio@14f02000` (L358) | `compatible="zte,zx279133-mdio"`<br>`status="disabled"` | `reg=<0x0 0x14f02000 0x0 0x1000>`<br>`clocks=<&lsp1_clk ZX279133_LSP1_CLK_MDIO1_PCLK>, <&lsp1_clk ZX279133_LSP1_CLK_MDIO1_WCLK>`<br>`clock-names="pclk", "wclk"`<br>`resets=<&lsp1_clk ZX279133_LSP1_RESET_MDIO1>` |
| `/soc/pwm@14f10000` (L370) | `compatible="zte,zx279133-pwm"`<br>`status="disabled"` | `reg=<0x0 0x14f10000 0x0 0x1000>`<br>`clocks=<&lsp1_clk ZX279133_LSP1_CLK_PWM_PCLK>, <&lsp1_clk ZX279133_LSP1_CLK_PWM_WCLK>`<br>`clock-names="pclk", "wclk"` |
| `/soc/efuse@14f11000` (L380) | `compatible="zte,zx279133-efuse"` | `reg=<0x0 0x14f11000 0x0 0x1000>`<br>`clocks=<&lsp1_clk ZX279133_LSP1_CLK_EFUSE_PCLK>, <&lsp1_clk ZX279133_LSP1_CLK_EFUSE_WCLK>`<br>`clock-names="pclk", "wclk"` |
| `/soc/efuse@14f11000/nvmem-layout` (L388) | `compatible="fixed-layout"` | — |
| `/soc/efuse@14f11000/nvmem-layout/calibration@44` (L393) | — | `reg=<0x44 0x4>` |
| `/soc/watchdog@14f09000` (L400) | `compatible="zte,zx279133-watchdog"`<br>`status="okay"` | `reg=<0x0 0x14f09000 0x0 0x1000>`<br>`clocks=<&lsp1_clk ZX279133_LSP1_CLK_WDT0_PCLK>, <&lsp1_clk ZX279133_LSP1_CLK_WDT0_WCLK>`<br>`clock-names="pclk", "wdtclk"` |
| `/soc/watchdog@14f0a000` (L414) | `compatible="zte,zx279133-watchdog"`<br>`status="disabled"` | `reg=<0x0 0x14f0a000 0x0 0x1000>`<br>`clocks=<&lsp1_clk ZX279133_LSP1_CLK_WDT1_PCLK>, <&lsp1_clk ZX279133_LSP1_CLK_WDT1_WCLK>`<br>`clock-names="pclk", "wdtclk"` |
| `/soc/phy@16000000` (L427) | `compatible="zte,zx279133-pon-serdes"`<br>`status="disabled"` | `reg=<0x0 0x16000000 0x0 0x10000>, <0x0 0x16010000 0x0 0x10000>, <0x0 0x10e100c0 0x0 0x8>`<br>`reg-names="serdes", "pll", "topcrm-mode"`<br>`clocks=<&topcrm ZX279133_TOPCRM_CLK_PON_SERDES_PCLK>`<br>`clock-names="pclk"`<br>`resets=<&local_reset ZX279133_LOCAL_RESET_PON_SERDES>, <&local_reset ZX279133_LOCAL_RESET_PON_SERDES_APB>`<br>`reset-names="serdes", "apb"` |
| `/soc/syscon@17000080` (L443) | `compatible="zte,zx279133-pon-route", "syscon"` | `reg=<0x0 0x17000080 0x0 0x4>` |
| `/soc/phy@16100000` (L448) | `compatible="zte,zx279133-uni-serdes"`<br>`status="disabled"` | `reg=<0x0 0x16100000 0x0 0x100000>`<br>`clocks=<&topcrm ZX279133_TOPCRM_CLK_UNI_SERDES_PCLK>, <&topcrm ZX279133_TOPCRM_CLK_UNI_SERDES_50M>`<br>`clock-names="pclk", "ref50m"`<br>`resets=<&local_reset ZX279133_LOCAL_RESET_UNI_SERDES_RX>, <&local_reset ZX279133_LOCAL_RESET_UNI_SERDES_TX>`<br>`reset-names="rx", "tx"` |
| `/soc/ethernet@19000000` (L461) | `compatible="zte,zx279133-nppt"`<br>`status="disabled"` | `reg=<0x0 0x19000000 0x0 0x1000000>, <0x0 0x18000000 0x0 0x1000000>`<br>`reg-names="nppt", "pps"`<br>`interrupts=<GIC_SPI 5 IRQ_TYPE_LEVEL_HIGH>, <GIC_SPI 1 IRQ_TYPE_LEVEL_HIGH>, <GIC_SPI 2 IRQ_TYPE_LEVEL_HIGH>, <GIC_SPI 3 IRQ_TYPE_LEVEL_HIGH>, <GIC_SPI 4 IRQ_TYPE_LEVEL_HIGH>`<br>`interrupt-names="nppt", "rx", "idm", "buffer-release", "local-test"`<br>`clocks=<&topcrm ZX279133_TOPCRM_CLK_PON_IDM_ACLK>, <&topcrm ZX279133_TOPCRM_CLK_PON_TM_ACLK>, <&topcrm ZX279133_TOPCRM_CLK_PON_PCLK>, <&topcrm ZX279133_TOPCRM_CLK_PON_SMAC_WCLK>, <&topcrm ZX279133_TOPCRM_CLK_PON_WOE1_WCLK>, <&topcrm ZX279133_TOPCRM_CLK_PON_MAC_WCLK>`<br>`clock-names="idm-aclk", "tm-aclk", "pclk", "smac-wclk", "woe1-wclk", "mac-wclk"`<br>`phys=<&pon_serdes>, <&uni_serdes>`<br>`phy-names="serdes", "lan-serdes"`<br>`pcs-handle=<&xpcs1>`<br>`phy-mode="2500base-x"` |
| `/soc/ethernet-pcs@1a000000` (L491) | `compatible="snps,dw-xpcs"`<br>`status="disabled"` | `reg=<0x0 0x1a000000 0x0 0x800000>`<br>`reg-names="direct"`<br>`clocks=<&topcrm ZX279133_TOPCRM_CLK_UNI_SERDES_PCLK>`<br>`clock-names="csr"` |
| `/soc/ethernet-pcs@1b000000` (L501) | `compatible="snps,dw-xpcs"`<br>`status="disabled"` | `reg=<0x0 0x1b000000 0x0 0x800000>`<br>`reg-names="direct"`<br>`clocks=<&topcrm ZX279133_TOPCRM_CLK_PON_SERDES_PCLK>`<br>`clock-names="csr"` |

## Mainline SR1010 board overrides and wiring
| Node (source line) | compatible / status | resources & wiring |
|---|---|---|
| `/` (L10) | `compatible="zte,zx279133-sr1010", "zte,zx279133"` | — |
| `/memory@80000000` (L24) | — | `reg=<0x0 0x80000000 0x0 0x20000000>` |
| `/reserved-memory` (L29) | — | `ranges=true` |
| `/reserved-memory/buffer@9e100000` (L34) | — | `reg=<0x0 0x9e100000 0x0 0x1000000>` |
| `/reserved-memory/buffer@9d700000` (L39) | — | `reg=<0x0 0x9d700000 0x0 0xa00000>` |
| `/reserved-memory/buffer@9f100000` (L44) | — | `reg=<0x0 0x9f100000 0x0 0x0f00000>` |
| `/gpio-keys` (L50) | `compatible="gpio-keys"` | `label="SR1010 reset button"` |
| `/gpio-keys/button-reset` (L54) | — | `gpios=<&gpio0 0 GPIO_ACTIVE_LOW>`<br>`label="reset"` |
| `/leds` (L62) | `compatible="gpio-leds"` | — |
| `/leds/led-white` (L65) | — | `gpios=<&gpio2 0 GPIO_ACTIVE_LOW>`<br>`function=LED_FUNCTION_STATUS`<br>`label="sr1010:white"` |
| `/leds/led-red` (L74) | — | `gpios=<&gpio2 4 GPIO_ACTIVE_LOW>`<br>`function=LED_FUNCTION_STATUS`<br>`label="sr1010:red"` |
| `/thermal-zones/soc-thermal` (L85) | — | `thermal-sensors=<&pvt>` |
| `/&uart0` (L91) | `status="okay"` | — |
| `/&usb3` (L95) | `status="okay"` | — |
| `/&pvt` (L99) | `status="okay"` | — |
| `/&pon_serdes` (L103) | `status="okay"` | — |
| `/&uni_serdes` (L107) | `status="okay"` | — |
| `/&nppt` (L111) | `status="okay"` | `memory-region=<&bmu_reserved>, <&se_hash_reserved>, <&idm_reserved>`<br>`memory-region-names="bmu", "se-hash", "idm"`<br>`phy-handle=<&zx279051>` |
| `/&nppt/ethernet-lan-cpu` (L117) | `compatible="zte,zx279133-lan-conduit"` | — |
| `/&nppt/switch` (L121) | `compatible="zte,zx279133-rtl8372n"` | `reset-gpios=<&gpio1 1 GPIO_ACTIVE_LOW>`<br>`mdio-handle=<&rtl8372n_smi>` |
| `/&nppt/switch/ports/port@4` (L130) | — | `reg=<4>`<br>`label="lan4"`<br>`phy-mode="internal"` |
| `/&nppt/switch/ports/port@4/fixed-link` (L135) | — | `speed=<2500>` |
| `/&nppt/switch/ports/port@5` (L141) | — | `reg=<5>`<br>`label="lan3"`<br>`phy-mode="internal"` |
| `/&nppt/switch/ports/port@5/fixed-link` (L146) | — | `speed=<2500>` |
| `/&nppt/switch/ports/port@6` (L152) | — | `reg=<6>`<br>`label="lan2"`<br>`phy-mode="internal"` |
| `/&nppt/switch/ports/port@6/fixed-link` (L157) | — | `speed=<2500>` |
| `/&nppt/switch/ports/port@7` (L163) | — | `reg=<7>`<br>`label="lan1"`<br>`phy-mode="internal"` |
| `/&nppt/switch/ports/port@7/fixed-link` (L168) | — | `speed=<2500>` |
| `/&nppt/switch/ports/port@8` (L174) | — | `reg=<8>`<br>`phy-mode="10gbase-r"` |
| `/&nppt/switch/ports/port@8/fixed-link` (L179) | — | `speed=<10000>` |
| `/&mdio0` (L188) | `status="okay"` | — |
| `/&mdio0/switch-controller@1d` (L191) | `compatible="realtek,rtl8372n"` | `reg=<0x1d>` |
| `/&mdio1` (L197) | `status="okay"` | — |
| `/&mdio1/ethernet-phy@1` (L200) | `compatible="ethernet-phy-id0000.84b9"` | `reg=<1>`<br>`reset-gpios=<&gpio0 1 GPIO_ACTIVE_LOW>` |
| `/&xpcs0` (L210) | `status="okay"` | — |
| `/&xpcs1` (L214) | `status="okay"` | — |
| `/&watchdog1` (L218) | `status="okay"` | — |
| `/&spifc` (L222) | `status="okay"` | — |

## Architecture / data flow

Firmware starts from the FIT DTB and U-Boot enables the serial-flash controller plus injects rootfs/version metadata. The vendor kernel consumes many private `zxic,*` clock leaf providers and private platform compatibles. Mainline replaces that clock forest with TOPCRM and LSP0/LSP1 providers, explicit reset controllers, fixed firmware-owned PLL roots, and supported subsystem drivers. On SR1010, NPPT is the central packet/data-plane node: it consumes PON and UNI SerDes PHYs, XPCS1 for PON and XPCS0 for LAN, three reserved-memory pools, a ZX279051 external PHY, and a logical LAN conduit to an RTL8372N switch. The switch's ports 4-7 are fixed 2.5G LAN links and port 8 is a fixed 10G CPU link.

## Files retrieved / start here

1. `vendor-reference/2b5/zx279133-sr1010.dts` lines 1-1779 — complete FIT/vendor inventory and board defaults.
2. `vendor-reference/2b5/zx279133-sr1010.dtb` — FIT binary verified by decompilation.
3. `vendor-reference/sr1010-vendor-runtime/device-tree/runtime.dtb` — live U-Boot-fixed tree; only three property differences listed above.
4. `vendor-reference/sr1010-vendor-runtime/device-tree/proc-device-tree.tar` — captured live procfs DT evidence.
5. `linux-6.18.38/arch/arm64/boot/dts/zte/zx279133.dtsi` lines 1-511 — mainline SoC resources.
6. `linux-6.18.38/arch/arm64/boot/dts/zte/zx279133-sr1010.dts` lines 1-237 — board wiring, enables, memory, switch and flash layout.

Start with `linux-6.18.38/arch/arm64/boot/dts/zte/zx279133.dtsi` to understand the supported hardware model, then use the vendor table to identify omitted/unsupported BSP blocks.

