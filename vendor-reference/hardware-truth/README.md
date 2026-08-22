# SR1010 / ZX279133 “硬件真相”

本目录把四类证据放在同一条链上：

1. **DTS/DTB**：官方 2B5 FIT DTB、U-Boot fixup 后的运行时 DTB、当前 mainline DTS；
2. **DT binding / mainline driver**：当前 `linux-6.18.38` 中的 ZX279133/SR1010 binding 和驱动；
3. **TRM/数据手册**：可公开获得的厂商资料及其缺口；
4. **vendor kernel/kmod**：49 个 `.ko` 路径、28 个唯一 ELF、13 个实际加载模块、运行时 sysfs/procfs/debugfs/IRQ/clock/MTD/网络状态，以及 vendor 模块反汇编。

自动化清单覆盖 **402 个 DT 资源节点**和 **49 个 ZX279133/SR1010 mainline 源文件**。

## 结论摘要

### 能确认的硬件

- SoC 是 **ZX279133，双 Cortex-A53，512 MiB RAM**。
- 板载数据面是：

```text
CPU / Linux
  <-> IDM descriptor/ring
  <-> NPPT + PPS packet engine
  <-> XMAC0/XPCS0/Uni-SerDes -> RTL8372N -> 4 x 2.5G LAN
  <-> XMAC1/XPCS1/PON-SerDes -> ZX279051 -> 2.5G WAN
```

- 运行时明确检测到的交换芯片是 **RTL8372N**，不是模块文件名暗示的 RTL8373。`rtl8373_switch.ko` 是 SDK/模块族名称。
- ZX279051 的私有 ID 是 `0x000084b9`；Clause-22 reg 26 的 bit6 是 link，`[9:7]` 是速度/双工编码。
- `rlt8226b.ko` 被加载并初始化，但现有证据**不能证明板上另有一颗独立 RTL8226B**；更可能是 SDK/交换芯片 PHY 支持。
- Flash 是 **Winbond W25N01-class 1-Gbit SPI-NAND**：128 MiB、128 KiB erase、2 KiB page、64 B OOB。精确 GV/KV/后续同 ID 版本仍不确定。

### vendor DT 与真实运行状态

官方 FIT DTB 与 live DTB 只有三处逻辑差异：

1. `/soc/spifc@0x10d0f000/status`: `disabled` -> `okay`；
2. `/chosen/bootargs` 改为从 `/dev/mtdblock8` 的 JFFS2 启动；
3. `/chosen/versioninfo` 由 U-Boot 填充。

IRQ、寄存器区、clock/reset、OPP 和板级连线没有其它变化。因此，vendor DTS 的地址图可信度很高，但它仍包含 malformed PCIe `reg`、非标准 unit-address、拼写错误和重叠分区等官方缺陷。

### Linux framework 使用真相

| 类别 | 硬件/运行时真相 |
|---|---|
| DMA | vendor DT 没有 `dmas`；NPPT/IDM 直接编程物理地址和保留内存，绕过常规 DMA API。通用 DMA API 只在 `tsi721_mport` 和 USB/SCSI 路径被明确看到。 |
| IOMMU | DTS、运行时和 vendor 模块均无 Linux IOMMU 接入证据；vendor `SMMU` 名称指 NP/PPS 内部块，不能等同于 ARM SMMU。 |
| clock | vendor CCF 描述大量时钟，但 PON 工作时相关 CCF leaf 仍显示 enable=0，说明 vendor 驱动还直接操作 TOPCRM。 |
| reset | top reset `0x10e10060`、local reset `0x10e10070` 已绑定；网络/SerDes 驱动同时存在直接 TOPCRM reset 操作。 |
| regulator | 运行时只有 `regulator-dummy`；CPU PWM regulator 一直 `-EPROBE_DEFER (-517)`。不能声称 CPU 电压调节可用。 |
| power-domain | 无 DT/genpd/运行时可用证据。 |
| OPP/cpufreq | vendor DT 有 500–1300 MHz CPU OPP 表，但运行时 cpufreq 属性不可读、regulator 未工作；mainline 因安全原因删除 DVFS。 |
| interconnect | 无 DT 或驱动 framework 接入证据。 |
| PHY | vendor 使用私有 MDIO/APB/SerDes API，不使用 phylib 拓扑；mainline 才把 ZX279051、SerDes、XPCS 建模为标准 PHY/PCS。 |
| thermal | PVT probe 成功，时钟约 1.190477 MHz；未捕获 vendor thermal-zone/hwmon/trip 数据。 |
| PM | suspend 成功/失败均为 0，wakeup_sources 为空，所有 IRQ wake disabled；只能证明 PSCI/CPU suspend 被声明，不能证明整机 suspend 可用。 |
| debugfs | 没有 vendor `.ko` 调用 `debugfs_create*`。现有 debugfs 文件由 clock/GPIO/regulator/pinctrl/PM/USB 框架提供。 |

## 核心寄存器区

| Block | Physical base/range | 可信结论 |
|---|---:|---|
| GICv3 | `0x00500000`, `0x00540000` | vendor DT |
| LSP0 CRM | `0x10d00000` | UART/SFC/GPIO 低速域 |
| UART0 | `0x10d0d000` | live `uart-pl011`, Linux IRQ 14 |
| SFC | `0x10d0f000` | live SPI-NAND；vendor 100 MHz，mainline 暂限 25 MHz |
| GPIO0..4 | `0x10d10000 + n*0x40` | 5 x 16 GPIO，GPIO 0–79 |
| sysctrl | `0x10e00000` | CCI/系统控制，部分语义来自反汇编 |
| TOPCRM | `0x10e10000` | mux/divider/gate；`+0x60` top reset，`+0x70` local reset |
| pinctrl | `0x10e20000` | live 只有 SFC pins 52–57 被占用 |
| PCU | `0x10e30000` | 低功耗/系统控制块，语义主要来自 vendor driver |
| PVT | `0x10e70000` | live PVT/thermal probe |
| LSP1 CRM | `0x14f00000` | MDIO/I2C/WDT/efuse 时钟域 |
| MDIO0/1 | `0x14f01000`, `0x14f02000` | live 2.5 MHz；私有 C22/C45 控制器 |
| I2C0/1 | `0x14f03000`, `0x14f04000` | live 25 MHz；pinctrl 获取失败 |
| WDT0/1 | `0x14f09000`, `0x14f0a000` | live 16 s heartbeat |
| efuse | `0x14f11000` | live driver bound；SerDes/PVT calibration 来源 |
| USB3 | `0x15010000` | live xHCI, Linux IRQ 19 |
| GE-PHY APB | `0x15400000` | vendor DT/driver，当前板级用途有限 |
| RGMII | `0x15900000` | vendor DT/driver，首选 WAN path 不依赖 |
| PON SerDes/PLL | `0x16000000`, `0x16010000` | XMAC1/WAN path |
| Uni-SerDes | `0x16100000` | XMAC0/RTL8372N CPU link |
| PON | `0x17000000` | live PON driver, Linux IRQ 21 |
| PPS | `0x18000000` | packet processing/search block |
| NPPT | `0x19000000` | 16 MiB aggregate datapath；XMAC0 `+0x140000`，XMAC1 `+0x180000`，IDM `+0x280000` |
| XPCS0/1 | `0x1a000000`, `0x1b000000` | vendor DT 给一个 32 MiB 大窗；mainline 按访问模式拆成两个 8 MiB PCS |

## 数据面保留内存与 IRQ

mainline 板级描述把 vendor 早期内存布局固化为：

- IDM: `0x9d700000`, 10 MiB；
- SE hash: `0x9e100000`, 16 MiB；
- BMU: `0x9f100000`, 15 MiB。

网络相关 IRQ：

| Linux IRQ | GIC HWIRQ | action |
|---:|---:|---|
| 21 | 38 | `pon` |
| 25 | 37 | `nppt` |
| 26 | 33 | `cpu` / IDM RX |
| 27 | 34 | `idm` |
| 28 | 35 | `buf_rls` |
| 29 | 36 | `localtest` |

vendor 驱动还设置 IRQ affinity；这是软件策略，不是 DT 硬件属性。

## 用户可见 ABI

- 28 个唯一模块、全部 modinfo/alias/depends/parameter/API 分类：[`vendor-modules.md`](vendor-modules.md)、[`vendor-modules.csv`](vendor-modules.csv)。
- 完整 module parameter、procfs、**831 个 `np.ko` sysfs attribute symbol**、其它 sysfs/debugfs：[`interfaces.md`](interfaces.md)。
- 精确 ioctl major/command map：[`ioctls.md`](ioctls.md)。重点：
  - `TMI7604`: major 107, cmds `0x33/0x34`；
  - `peripheral`: major 106, 17 个 raw command；
  - `np`: major 191, cmds `0x800..0x821`；
  - `zx_ponreg`: major 222, cmds `0/1/3/0x4004de03`；
  - `switch.ko` 的 netdev ioctl 在该固件中只是 `return 0`。

## 证据索引

| 文件 | 内容 |
|---|---|
| [`vendor-modules.md`](vendor-modules.md) | 每个唯一 `.ko`、重复路径、SHA256、modinfo、符号、资源/API、加载状态 |
| [`interfaces.md`](interfaces.md) | module parameter、procfs、sysfs、debugfs、ioctl 索引 |
| [`ioctls.md`](ioctls.md) | ioctl 精确命令与行为 |
| [`device-tree.md`](device-tree.md) | vendor FIT/live/mainline 每个 compatible/resource 节点和差异 |
| [`mainline-drivers-bindings.md`](mainline-drivers-bindings.md) | 每个 ZX279133 driver/binding 的 compatible、IRQ、clock/reset、PM、寄存器 |
| [`runtime-evidence.md`](runtime-evidence.md) | 实际 binding、IRQ、iomem、clock、regulator、GPIO/pinctrl、thermal、PM、MTD、网络 |
| [`trm-datasheet-correlation.md`](trm-datasheet-correlation.md) | 公开资料、反向寄存器语义、冲突和未知项 |
| [`inventory.json`](inventory.json) | 机器可读总清单 |
| [`dt-nodes.csv`](dt-nodes.csv) | 402 个 DT 资源节点 |
| [`mainline-sources.csv`](mainline-sources.csv) | 49 个 ZX279133/SR1010 mainline 源/绑定文件 |

## 可信度规则

- **Verified**：至少两种独立证据，例如 vendor DT + live runtime，或 vendor binary + 板级验证；
- **Strong**：一个直接一手证据，且与其它观察一致；
- **Inference**：由访问顺序、符号名、寄存器值推断，不应直接固化为 binding ABI；
- **Unknown**：无足够证据。

当前 mainline 代码是“实现/验证证据”，不是原始芯片规格。没有公开 ZX279133、ZX279051、RTL8372N 完整 TRM；因此 reset value、reserved bit、电气/时序极限、全部 SerDes/NPPT 位定义仍只能通过 NDA 文档、总线 trace 或更多实机实验补齐。

## 重建清单

```sh
cd vendor-reference
python3 hardware-truth/extract_inventory.py
```

脚本只读 vendor artifacts，重新生成 `inventory.json`、`vendor-modules.csv`、`dt-nodes.csv` 和 `mainline-sources.csv`。
