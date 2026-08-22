# Runtime hardware truth — SR1010 vendor image

Scope: all 950 regular files under `vendor-reference/sr1010-vendor-runtime` were enumerated. Evidence below distinguishes instantiated/bound hardware from merely registered drivers. Snapshot is Linux 5.4.196; primary manifests are `metadata.txt`, `system/uname.txt`, `system/proc/*`, `buses/*`, `debugfs/*`, `irq/*`, `mtd/*`, `network/*`, and `kernel/dmesg.txt`.

## Loaded modules

| module | bytes | users/dependencies | state/taint |
|---|---:|---|---|
| xdpi | 1,015,808 | 0 | Live, O |
| switch | 176,128 | 0 | Live, O |
| zx_ponreg | 16,384 | switch | Live, O |
| peripheral | 16,384 | switch | Live, O |
| cpu_ctrl | 20,480 | switch | Live, O |
| np | 2,719,744 | switch, peripheral | Live, O |
| plat_132 | 253,952 | switch, zx_ponreg, peripheral, np | Live, O |
| rtl8373_switch | 589,824 | peripheral, np; permanent | Live, O |
| TMI7604 | 16,384 | 0 | Live, O |
| rlt8226b | 57,344 | 0 | Live, O |
| zx279051 | 36,864 | plat_132, rlt8226b | Live, O |
| shellproc | 45,056 | permanent | Live, O |
| bspdriver | 16,384 | peripheral, plat_132, TMI7604 | Live, O |

Source: `system/proc/modules`. Per-module metadata/parameters are under `modules/loaded/` (notably `np/parameters/g_np_reg_print_level`, `plat_132/parameters/g_pon_work_mode`, `rlt8226b/parameters/rtldebug`). Snapshot omission: `modules/loaded/` has no directories for live `switch` or `bspdriver`, despite `/proc/modules` proving both loaded.

## Instantiated devices and actual bindings

| device(s) | bound driver | runtime fact |
|---|---|---|
| `10d0d000.serial` | `uart-pl011` (AMBA) | console ttyAMA0, MMIO 10d0d000, IRQ 14 |
| `10d0f000.spifc` | `zx-spifc` | active 100 MHz SPI-NAND controller |
| `10d10000/40/80/c0/100.gpio` | `zx_gpio` | five 16-line chips, GPIO 0–79 |
| `10e10060.toprst`, `10e10070.localrst` | `zx2967-reset` | reset controllers bound |
| `10e20000.pinctrl` | `zxic-pinctrl` | controller bound; range parsing errors |
| `10e70000.pvt` | `zx_pvt_sensor_cln22ulp` | PVT/thermal probe OK, clock 1,190,477 Hz |
| `14f01000.mdio`, `14f02000.mdio` | `zx_mdio` | both probe OK at 2.5 MHz; pin acquisition failed |
| `14f03000.i2c`, `14f04000.i2c` | `zx_i2c` | adapters 0/1 probe OK at 25 MHz; pin acquisition failed |
| `14f09000.wdt`, `14f0a000.wdt` | `zx_wdt` | both started, heartbeat 16 s, driver reports clock 1024 |
| `14f11000.efuse` | `zx-efuse` | probe OK, 25 MHz |
| `15010000.usb3` | `xhci-hcd` | USB2 bus 1 + USB3 bus 2, IRQ 19 |
| `17000000.pon` | `zte,zx279133-pon` | IRQ 21; probe ultimately OK, mode 10/UP_WAN_P2P |
| `reg-dummy` | `reg-dummy` | sole successfully registered regulator |

Exact binding evidence: each `buses/devices/*/links.txt`; population: `buses/platform-devices.txt`, `buses/amba-devices.txt`; probe facts: `kernel/dmesg.txt`. Registered-but-not-proof-of-use driver names are in `buses/platform-drivers.txt` and must not be treated as bindings.

Unbound instantiated platform nodes include clock providers/CRMs, sys_ctrl, boot/reboot/rstctrl, `cpufreq-dt`, `np`, PSCI, board-info, key/LED/PON helper nodes, Wi-Fi/WOE/SE/BMU, and `soc:regulator@cpu`; see empty `driver=` entries in their `buses/devices/*/links.txt`.

## IRQ truth

| Linux IRQ | HWIRQ | trigger | action | CPU0 / CPU1 count | wake |
|---:|---:|---|---|---:|---|
| 3 | 27 | level | arch_timer | ~38.2k / ~38.2k | disabled |
| 14 | 63 | level | uart-pl011 | 24,748 / 0 | disabled |
| 15 | 75 | level | 14f03000.i2c | ~1.27M / 0 | disabled |
| 16 | 76 | level | 14f04000.i2c | 0 / 0 | disabled |
| 19 | 97 | level | xhci-hcd:usb1 | 0 / 0 | disabled |
| 20 | 65 | level | zx-spifc | 274,627 / 0 | disabled |
| 21 | 38 | level | pon | 0 / 0 | disabled |
| 25 | 37 | level | nppt | 0 / 0 | disabled |
| 26 | 33 | level | cpu | 268 / 0 | disabled |
| 27 | 34 | level | idm | 0 / 0 | disabled |
| 28 | 35 | level | buf_rls | 0 / 0 | disabled |
| 29 | 36 | level | localtest | 0 / 0 | disabled |

Allocated but actionless IRQs: 1/2/4–8 (edge), 9–13 (level, action `(null)`), 17/18/22–24 (edge), all zero count and wake disabled. Exact per-IRQ fields: `irq/<n>/{hwirq,chip_name,type,actions,per_cpu_count,wakeup}`; aggregate snapshot: `system/proc/interrupts`. Counts differ slightly between independently captured files, as expected on a live system. `/proc/interrupts` records zero errors.

## I/O memory

| region | owner |
|---|---|
| 10d0d000–10d0dfff | PL011 serial |
| 10d10000–10d1013f | five GPIO banks, each 0x40 |
| 10e10060–10e10063 | top reset |
| 10e10070–10e10077 | local reset |
| 10e20000–10e20fff | pinctrl |
| 10e70000–10e7ffff | PVT sensor |
| 14f01000–14f01fff / 14f02000–14f02fff | MDIO0/1 |
| 14f03000–14f03fff / 14f04000–14f04fff | I2C0/1 |
| 14f09000–14f09fff / 14f0a000–14f0afff | WDT0/1 |
| 14f11000–14f11fff | efuse |
| 15010000–15013fff | USB3/xHCI |
| 80000000–9fffffff | 512 MiB System RAM (subranges reserved as listed) |

Source: `system/proc/iomem`. Notably absent from iomem despite active/probed state: SPIFC and PON; platform `resource` files were unreadable, so iomem is incomplete (`errors.log`).

## Clocks (active leaves/branches)

| active clock | enable/prepare | rate Hz |
|---|---:|---:|
| pll_1376m_clk / clk32k768 | 1/1 | 1,376,256,000 / 32,768 |
| lsp1_32k | 2/2 | 32,768 |
| wdt0/1 divider + wclk | 1/1 each | 32,768 |
| pll_lsp_2000m_clk | 4/4 | 2,000,000,000 |
| clk50m → clk1m22 → tempsensor_wclk | 1/1 | 50M → 1,190,477 → 1,190,477 |
| clk100m | 3/3 | 100M |
| lsp1_100m | 2/2 | 100M |
| mdio0/1 divider + wclk | 1/1 each | 2,500,000 |
| SPIFC chain (`spifc_wclk_mux`…`sfc_wclk`) | 1/1 | 100,000,000 |
| NAND chain (`nand_wclk_mux`, `nand_xclk`, `nand_wclk`) | 1/1 | 100M, 100M, 25M |
| clk200m → sd mux/div | 1/1 | 200M → 50M |
| clk250m | 1/1 | 250M |
| sys_aclk / sys_pclk | 2/2 | 250M / 125M |
| usb_aclk / usb_pclk | 1/1 | 250M / 125M |
| lsp0_pclk → uart0_pclk | 1/1 | 125M |
| osc25m | 2/2 | 25M |
| lsp1_25m | 2/2 | 25M |
| i2c0/1 mux + wclk | 1/1 each | 25M |
| lsp0_25m → uart0 mux | 1/1 | 25M |
| uart0_wclk | enable 2, prepare 3 | 25M |

All other listed clocks are disabled (enable 0), including PON named clocks despite the PON driver being active. Full hierarchy/rates: `debugfs/clk_summary.txt`. Boot reports missing `zx-clock,pll-en-bit` properties for all three PLL nodes (`kernel/dmesg.txt`).

## Regulators, GPIO, pinctrl, thermal

| area | runtime truth |
|---|---|
| regulators | only `regulator-dummy`, use=1, 0 mV; CPU PWM regulator repeatedly defers/fails `Failed to get PWM: -517` |
| GPIO | gpiochip0..4 map GPIO 0–79; debugfs shows no requested line labels |
| pinmux | only pins 52–57 (`p3-9..p4-1`) are claimed, all by `10d0f000.spifc`, function `spifc`; every other pin unclaimed |
| pinctrl errors | `invalid pin list in gpio-range node` and `bank-range node`; MDIO0/1 and I2C0/1 all report failure to get pins |
| thermal | PVT driver enabled at raw temp `0x193`, `zx_thermal_init end`, probe OK at 1,190,477 Hz; no captured thermal-zone/hwmon sysfs values or trip points |

Sources: `debugfs/regulator_summary.txt`, `debugfs/gpio.txt`, `debugfs/pinctrl/.../pinmux-pins`, `debugfs/pinctrl/.../pinmux-functions`, `kernel/dmesg.txt`.

## PM / suspend / wakeup

| evidence | value |
|---|---|
| suspend successes/failures | 0 / 0; every stage-failure counter 0 |
| wakeup sources | empty table |
| per-IRQ wake | disabled for every captured IRQ |
| firmware PM | PSCI device exists; DT has CPU suspend parameters/SMC IDs |
| PM domains / cpufreq | `pm_genpd_summary` unreadable; all CPU cpufreq attributes unreadable |

Sources: `debugfs/suspend_stats.txt`, `debugfs/wakeup_sources.txt`, `irq/*/wakeup`, `errors.log`, runtime DT `device-tree/runtime.dtb`. Thus suspend capability is declared but no actual suspend/resume cycle or wake-capable device is evidenced.

## MTD / flash

Common geometry: NAND, erase 131,072, write 2,048, OOB 64, ECC 4 bits/512 B, flags `0x400`, zero bad blocks.

| MTD | name | bytes | corrected bits |
|---:|---|---:|---:|
| 0 | whole flash | 134,217,728 | 0 |
| 1 | bootloader | 1,048,576 | 0 |
| 2 | tags | 1,048,576 | 0 |
| 3 | usercfg | 2,097,152 | 0 |
| 4 | defcfg | 2,097,152 | 0 |
| 5 | kernel1 | 42,991,616 | 0 |
| 6 | kernel2 | 42,991,616 | 0 |
| 7 | rootfs1 | 42,991,616 | 0 |
| 8 | rootfs2 | 35,651,584 | 8 |
| 9 | Plugin | 41,943,040 | 6 |

SPIFC found Winbond 128 MiB SPI-NAND and created ten dynamic partitions. Root is `/dev/mtdblock8` JFFS2 read-only; mtd3, mtd4, mtd9 mounted RW at `/usercfg`, `/defcfg`, `/Plugin`. JFFS2 CRC failures were logged. Sources: `system/proc/mtd`, `mtd/mtd*/**`, `system/mount.txt`, `kernel/dmesg.txt`.

## Network / MDIO / PHY topology

| layer | observed topology/state |
|---|---|
| MDIO controllers | platform MDIO0 @14f01000 and MDIO1 @14f02000 bound/probed, 2.5 MHz |
| Linux MDIO devices | none enumerated (`buses/mdio-devices.txt` empty); only generic Clause 22/45 driver directories exist |
| external PHY/switch | vendor modules `rlt8226b`, `rtl8373_switch`, `zx279051`; dmesg finds RTL8372N/RL6818C, switch chip id 6; xmac5 PHY id 1 reaches 1000M full duplex |
| PON/internal netdevs | `sw`, `pon`, `idm` carrier up/operstate unknown; `oam` down |
| user LAN | eth0 carrier/up; eth1–eth4 enslaved to br0 and no-carrier/down; br0 192.168.5.1/24 and down/no-carrier |
| internal uplink | nbif0 192.168.2.2/24, carrier up, MTU 1480 |
| traffic at capture | eth0 RX 80,297 B; pon RX 85,015 B; nbif0 RX 84,519/TX 13,183 B; other Ethernet ports zero |

No netdev has a sysfs `device`, `driver`, `of_node`, or `phydev` link (`network/interfaces/*/links.txt`), so PHY-to-netdev mapping is implemented/hidden in proprietary modules rather than Linux phylib topology. Sources: `buses/mdio-*`, `network/ip-address.txt`, `network/proc-net-dev.txt`, `network/interfaces/*`, `kernel/dmesg.txt`.

## Runtime endpoints captured

| namespace | captured endpoints |
|---|---|
| procfs | cmdline, cpuinfo, devices, filesystems, interrupts, iomem/ioports, kallsyms, meminfo, misc, modules, mounts, mtd, partitions, kernel hardening sysctls, version under `system/proc/`; network proc views under `network/proc-net-*` |
| sysfs | bus device/driver inventories and per-device `uevent`, `modalias`, links/resources under `buses/`; IRQ fields under `irq/`; MTD attributes under `mtd/`; netdev attributes/links under `network/interfaces/`; loaded-module metadata under `modules/loaded/` |
| debugfs | clocks, GPIO, regulator, suspend stats, wakeup sources, USB, and pinctrl files under `debugfs/` |

Mounted namespaces are confirmed by `system/mount.txt` (proc, sysfs, debugfs, usbfs). There is no collected thermal class, hwmon, power-supply, devfreq, RTC, LED, input, I2C-client, or watchdog-class sysfs subtree.

## Discrepancies versus static DTS

Static baseline: `vendor-reference/2b5/zx279133-sr1010.dts`; runtime DT: `vendor-reference/sr1010-vendor-runtime/device-tree/runtime.dtb`.

| discrepancy | static DTS | runtime truth |
|---|---|---|
| SPIFC status | line 1323: `status = "disabled"` | runtime DT decompiles to `status = "okay"`; device bound, IRQ 20 active, pins claimed, SPI-NAND/MTD live |
| CPU regulator | lines 1205–1213 declare always-on PWM regulator `vcc0v8_a53` | platform node exists but unbound; probe repeatedly fails -517; debugfs exposes only dummy regulator |
| MDIO/I2C pinctrl | static nodes have default pinctrl references and named groups | all four drivers report failed pin acquisition; debugfs shows their pins unclaimed |
| pinctrl ranges | static lines 821–840 supply custom bank/GPIO range data | driver rejects both range node pin lists |
| MTD naming | static partition nodes use `tag`, `kern0/1`, `rootfs0/1` | live names are `tags`, `kernel1/2`, `rootfs1/2`; live sizes are authoritative |
| declared vs active clocks | DTS defines full PON/PCIe/etc. trees | PON runs while its named CCF clocks show enable=0; PCIe absent; only active clock subset above is evidenced |
| aliases vs devices | static aliases include serial1 and many disabled peripherals | only serial0 instantiated/bound; disabled peripherals correctly absent |

The runtime DT otherwise confirms SPIFC override as the key board/runtime mutation. `dtc` also reports malformed PCI child `reg` lengths, numerous unit-address naming violations, and SPI bus node naming violations when decompiling `runtime.dtb`; these are schema-quality gaps, not necessarily runtime failures.

## Gaps / collection errors

- `errors.log` records unreadable platform `resource`, NUMA/DMA-mask files for essentially every platform device, making `/proc/iomem` the only resource map and leaving active SPIFC/PON ranges unresolved.
- IRQ 27/9 details were partly unreadable/missing during collection; live aggregate and per-IRQ snapshots were asynchronous.
- MTD `failed` counters and most `mtd*ro` attributes unreadable; mtd1 directory absent even though `/proc/mtd` and errors prove it exists.
- PM genpd and CPU cpufreq sysfs entirely unreadable; no proof of actual DVFS or suspend.
- Network detailed-statistics and neighbour commands failed because BusyBox `ip` lacks required syntax; speed/duplex files returned `EINVAL` for many virtual/proprietary netdevs.
- Firmware inventory failed; `regulatory.db` is missing at runtime and cfg80211 repeatedly logs `-2`.
- Collection archive creation failed from no space; evidence directory itself is intact and SHA256-listed (`SHA256SUMS`).
- No Linux MDIO child devices/phydev symlinks, thermal zones, hwmon readings, GPIO consumers, wakeup sources, or non-dummy voltage readings were captured, so those relationships cannot be inferred beyond dmesg/vendor-module evidence.

