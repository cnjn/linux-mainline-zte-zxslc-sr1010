# SR1010 原厂 5.4 与当前主线 6.18 能力审计

审计日期：2026-08-31（Asia/Shanghai）

审计对象：

- 原厂固件：SR1010 V1.0.0.2B5，Linux 5.4.196。
- 当前主线：Linux 6.18.38，内核提交 `5336656ce7a3e7a8c18f8346284448b7ed5a25a7`，外层提交 `639322ad925eac62e2b8305e07a98e47f2cafa68`。
- 当前主线是 RAM 启动的 ZXDBG 诊断内核，不是完整产品发行配置。配置缺项必须区分“驱动尚未实现”和“诊断配置主动未启用”。

## 0. 结论先行

当前主线已经覆盖 SR1010 作为有线路由器所需的核心数据面：双核启动、时钟/复位、串口、GPIO、看门狗、MDIO、ZX279051、PON/UNI SerDes、XPCS、RTL8372N DSA、WAN/LAN 收发、NAPI/page-pool、IPv4/IPv6 软件路由、桥和接入口 VLAN、PPPoE、IPv4 NAT、IPv4/IPv6/PPPoE 硬件 flow offload、硬件统计/老化、DDR 大流表、XDP/AF_XDP 接口、EEE，以及一个可读写时间但不能调频、不能做包时间戳的基础 PHC。

它还不是原厂产品内核的等价替代品，缺口分为四类：

1. **产品控制面缺口**：原厂 FFE/CSPKernel、URL/DNS/应用识别、私有 QoS/ACL/meter、多播控制、DPI、私有 proc/sysfs/ioctl、诊断/测试接口没有复刻。
2. **PON 产品栈缺口**：主线有 SerDes/PHY 物理层，但没有 GPON/EPON/XG(S)-PON MAC、OMCI、OAM、DBA 和光模块管理栈。
3. **通用 Linux 产品配置缺口**：当前诊断配置没有 cgroup、namespace、seccomp、perf、kexec、XFRM/IPsec、完整 crypto、JFFS2/SquashFS/UBIFS/NTFS3/FUSE/overlay、I2C、PWM、suspend/cpufreq/regulator 等。
4. **通用 NIC/交换机缺口**：RSS/多队列、可调 ring/coalescing/channels/RSS、WoL、动态 RX 地址过滤、jumbo 9K、DSA devlink 和部分高级 QoS 仍缺失或未实现；RTL8372N 已有专用 offline self-test 和内部 PHY 线缆诊断。

因此，可信的表述是：**有线路由核心路径已达到可用且有较强板上验收证据；完整原厂产品能力、完整通用 NIC 能力和完整产品内核配置均未达到。**

## 1. 证据等级与统计口径

|等级|含义|
|---|---|
|A|当前或原厂实机运行快照/板上验收，且结果保存在仓库文档或采集目录中|
|B|当前配置、源码、DT/binding、构建产物共同证明；没有同等强度的板上功能验收|
|C|二进制符号、模块元数据或配置证明代码存在，但模块未加载或没有运行证据|
|D|只有名称/推断，或现有证据不足，不能宣称能力成立|

重要口径：

- “模块文件存在”不等于“SR1010 实机正在使用”。原厂目录有 27 个 `.ko`，实机快照只加载 13 个。
- `CONFIG_FOO=y` 证明代码编入，不证明硬件路径已工作。
- `in_hw`/`[HW_OFFLOAD]` 证明规则进入硬件；真正的硬件命中还需硬件计数、CPU/softirq、端点计数和吞吐等交叉证据。
- 原厂模块函数家族统计是对符号名的非互斥匹配，同一函数可能同时计入多个家族；它用于说明职责范围，不是业务功能数量。

## 2. 版本、构建物和总量统计

### 2.1 原厂内核本体

|项目|结果|
|---|---:|
|内核版本|Linux 5.4.196，`#2 SMP Tue Jun 9 14:23:49 CST 2026`|
|编译器|GCC 5.3.1 20160412，Buildroot 2017.05|
|CPU|2 × Cortex-A53（ARM implementer `0x41`、part `0xd03`、revision 4）|
|内存|命令行 `mem=512M`；运行时 `MemTotal=456972 kB`|
|原厂内核 ELF|15,327,763 字节；SHA-256 `35941b131502ba785326692169dc3b9ca165d4417ee50632dc335e7a856baa6d`|
|内核 CONFIG|3,993 个配置项；其中 3,988 个唯一配置名|
|唯一配置状态|`y=1442`、`m=10`、`n=2463`、数值/字符串等 73|
|核心 kallsyms 行|75,538|
|排除的模块 kallsyms 行|24,455|
|核心可执行函数/地址实体|41,983|
|可由上游谱系解释|39,235（93.45%）|
|相对原版 5.4.196 的厂商差异候选|1,101（2.62%）|
|明确 NTFS3 回移植|331（0.79%）|
|厂商关联项|309（0.74%）|
|尚未解决|1,007（2.40%）|

上面函数统计来自经过校验的分析产物：[FINAL_REPORT.md](/Volumes/code/sr1010-kernel/analysis/vendor-kernel-match/results-final/FINAL_REPORT.md)。官方 Linux 5.4.196 源码树校验了 65,732 个文件/符号链接，缺失、内容不同、额外文件均为 0；分析自身的 8 项一致性检查全部通过。

### 2.2 原厂外置模块

|项目|全部 27 个模块|实机加载 13 个模块|
|---|---:|---:|
|文件字节|8,388,632|7,948,960|
|`.text` 字节|2,177,608|2,078,400|
|定义函数|9,958|9,461|
|导出符号|1,238|1,192|
|运行时已分配 core 大小|不适用|4,980,736 字节|

“文件字节”和“运行时 core 大小”不是同一口径。`.ko` 文件还包含重定位、符号表、字符串和 modinfo；例如 `np.ko` 文件是 4,652,720 字节，但 `.text` 是 1,097,616 字节。

### 2.3 当前主线内核

|项目|结果|
|---|---:|
|内核版本|Linux 6.18.38+，构建号 `#353 SMP Mon Aug 31 13:43:38 UTC 2026`|
|编译器|GCC 12.2.0，GNU ld 2.40|
|内核提交|`5336656ce7a3e7a8c18f8346284448b7ed5a25a7`|
|实际构建 `.config`|2,231 个唯一配置名：`y=720`、`m=2`、`n=1437`、其他 72|
|`modules.builtin`|98 项：驱动 53、网络 29、文件系统 10、lib 4、crypto 2|
|可加载模块|2 个，共 47,888 字节|
|`vmlinux`|26,765,864 字节；`.text` 6,365,184 字节；25,939 个定义函数|
|SR1010/ZX279133 生产路径源码|29 个 C/H/DTS 文件，18,794 物理行，16,497 非空行|
|ZTE 网络核心源码|13 个 C/H 文件，12,681 物理行，11,276 非空行|
|最终 FIT|10,267,336 字节；SHA-256 `952ab576f722ee2655d2f0dba6abb35ea80007aab8e8f32e8b0f4f3fa6ff361a`|

实际构建配置以 [out/kernel/.config](/Volumes/code/zx279133/out/kernel/.config) 为准，不以 seed 配置文件直接代替。`olddefconfig` 和依赖选择使它与 `zxdbg.fragment` 有 256 个语义差异。

## 3. 原厂内核本体有哪些能力

### 3.1 通用内核与运行环境

|能力|原厂状态|证据|等级|
|---|---|---|---|
|双核 SMP|2 个 Cortex-A53 在线；`CONFIG_SMP=y`、`NR_CPUS=2`|cpuinfo/config|A|
|调度|100 Hz、voluntary preemption|`CONFIG_HZ=100`、`CONFIG_PREEMPT_VOLUNTARY=y`|B|
|模块|支持加载、卸载；配置中 10 项为 `m`|config + 13 个实机加载模块|A|
|cgroup|v1/v2 框架；实机挂载 blkio、cpu、cpuacct、cpuset、devices、freezer、memory、net_cls、net_prio、perf_event|mount/config|A|
|namespace|`CONFIG_NAMESPACES=y`|config；没有容器工作负载验收|B|
|seccomp|`CONFIG_SECCOMP=y`、filter 启用|config|B|
|性能事件|`CONFIG_PERF_EVENTS=y`、ARM HW perf events|config|B|
|kexec|`CONFIG_KEXEC=y`|config；没有实机 kexec 验收|B|
|BPF syscall/JIT|均关闭|config|A（明确缺失）|
|整机休眠|配置声明 suspend/CPU idle；运行快照成功/失败计数均为 0，无 wake source|config/runtime|D：不能宣称可用|
|DVFS|配置有 cpufreq/OPP/PWM regulator；实机 regulator `-EPROBE_DEFER`，cpufreq 属性不可读|runtime|D：声明存在、运行未证实|

### 3.2 文件系统、闪存和块设备

原厂运行时注册：ext2/ext3/ext4、squashfs、vfat、ntfs3、JFFS2、FUSE、overlay、exFAT、UBIFS，以及 tmpfs/ramfs/proc/sysfs/debugfs/cgroup 等伪文件系统。

|能力|状态|实机事实|等级|
|---|---|---|---|
|128 MiB SPI-NAND|可用|Winbond NAND，erase 128 KiB、page 2 KiB、OOB 64、ECC 4 bit/512 B|A|
|动态 MTD 分区|10 个|whole flash、bootloader、tags、usercfg、defcfg、kernel1/2、rootfs1/2、Plugin|A|
|JFFS2 根文件系统|可用|`/dev/mtdblock8` 只读根；mtd3/4/9 读写挂载|A|
|SquashFS 模块盘|可用|`/dev/loop0` 挂载到 `/kmodule`|A|
|NTFS3|内建回移植|331 个明确回移植函数实体；`CONFIG_NTFS3_FS=y`|B；未见磁盘挂载验收|
|USB mass storage|模块文件存在但快照未加载|`usb-storage.ko` 库存模块|C|

原厂日志出现 JFFS2 CRC 错误；这不否定驱动能力，但说明采集时闪存内容存在可靠性告警。

### 3.3 板级控制器与外设

|控制器/能力|原厂实机状态|等级|
|---|---|---|
|PL011 UART|console `ttyAMA0`，IRQ 14|A|
|TOP/local reset|两个 reset controller 绑定|A|
|GPIO|5 组，共 GPIO 0–79；驱动绑定，但采集时没有请求标签|A（控制器）；D（具体 LED/按键）|
|pinctrl|控制器绑定；SFC pins 52–57 被占用；range 节点有解析错误|A/部分|
|PVT/thermal|PVT probe 成功，时钟 1,190,477 Hz；没有采到 thermal-zone/hwmon 数值|A（探测）；D（温控策略）|
|MDIO|MDIO0/1 均绑定，2.5 MHz；Linux phylib 子设备为空，PHY 拓扑藏在私有模块|A|
|I2C|I2C0/1 均绑定，25 MHz；均报告获取 pin 失败|A（adapter）；部分|
|watchdog|WDT0/1 均启动，heartbeat 16 秒|A|
|eFuse|probe 成功，25 MHz|A|
|USB xHCI|USB2 bus 1 + USB3 bus 2，IRQ 19|A（控制器）；没有外设传输证据|
|PON platform|`17000000.pon` probe 成功，mode 10/`UP_WAN_P2P`|A（驱动）；没有光接入业务验收|
|PCIe|内核存在 ZX279133 PCIe 代码簇，但运行快照未实例化|C|
|RapidIO|内核和库存模块有支持，运行快照未加载相关模块|C|

### 3.4 原厂网络、过滤和产品框架

原厂内核本体相对原版 5.4.196 的主要差异簇如下。这里统计的是核心内核函数实体，不包含外置模块：

|差异簇|实体数|可证明的职责|
|---|---:|---|
|FFE/NPU/QoS|479|FFE flow、NPU、硬件 QoS、协议/队列初始化|
|ZX27913x SoC|305|PCIe、MDIO/PHY、clock、cpufreq、watchdog、SPI-NAND/MTD|
|厂商 netfilter/networking|231|URL/DNS filter、应用识别、NAT/fullcone、桥、conntrack 扩展|
|CSPKernel|207|内核—用户态消息、日志、ioctl、QoS 控制|
|WLAN integration|103|WLAN proc、LED、配置映射和平台整合，不等于无线数据面驱动已加载|
|BBX diagnostics|28|黑匣子/诊断记录|
|product platform|25|LED/按键、产品初始化|
|PON optical|12|光模块 I2C、功率/电流/温度/电压|
|switch/PHY/MDIO|8|Realtek PHY 初始化和链路控制|
|IDM messaging|6|IDM reserved memory/config access|
|NTFS3 backport|337|文件系统回移植，不属于 ZTE 产品功能原创|

### 3.5 原厂协议和安全能力

|能力|原厂状态|等级|
|---|---|---|
|IPv4/IPv6|启用|B；基础网络运行可见|
|bridge/VLAN/PPP/PPPoE|启用|A/B；实际 br0、nbif0、PON/内部 netdev 可见|
|legacy iptables/ip6tables/ebtables|广泛启用|B|
|conntrack helpers|FTP、H323、PPTP、SIP、TFTP、GRE 等启用|B|
|XFRM/ESP/IPsec core|启用|B|
|厂商 `ipsec.ko` 硬件 datapath|模块文件存在但未加载|C，不能算当前运行能力|
|crypto core|AES、DES、SHA、MD5、HMAC、RSA、DRBG 等启用|B|
|XDP/AF_XDP|没有对应原厂配置/接口证据|D/缺失|

## 4. 原厂 27 个外置模块逐项清单

数值列依次为：`.ko` 文件字节 / `.text` 字节 / 定义函数 / 导出符号。`已加载=否` 表示“采集目录中存在，但 `/proc/modules` 快照没有加载”，不能当作 SR1010 当时的活动能力。

|模块|已加载|数值|职责与能力|结论|
|---|---|---:|---|---|
|`TMI7604.ko`|是|8,888 / 1,324 / 13 / 0|major 107 `pse_dev`，`pse_ioctl` 接受 `0x33/0x34`|活动的板级 PSE 字符设备，A|
|`bspdriver.ko`|是|16,744 / 3,220 / 22 / 17|导出 BOB I2C 读写和板级 API|活动依赖库，A/B|
|`cpu_ctrl.ko`|是|25,416 / 4,280 / 25 / 6|proc 电源控制、CPU frequency 操作、USB suspend/resume、power-off hooks|活动，但不等于 DVFS 已工作，A/部分|
|`crypto_engine.ko`|否|12,064 / 1,864 / 19 / 14|Linux crypto-engine framework|库存，C|
|`idt_gen2.ko`|否|9,704 / 2,200 / 17 / 0|IDT RapidIO switch/sysfs attribute|库存，C|
|`idtcps.ko`|否|7,416 / 992 / 12 / 0|IDT CPS RapidIO switch driver|库存，C|
|`ipsec.ko`|否|46,256 / 14,932 / 62 / 0|AH/ESP encode/decode、线程 IRQ、厂商 crypto datapath|库存，不能算活动 IPsec 硬件卸载，C|
|`lan_test.ko`|否|22,136 / 5,836 / 28 / 1|procfs LAN HLT/串口化测试接口|库存测试模块，C|
|`np.ko`|是|4,652,720 / 1,097,616 / 4,555 / 774|NPPT/PPU/SE/FAST、TM/QoS、ACL、CPU packet I/O、PON、统计、低功耗、调试测试|原厂最大核心模块，A/B|
|`peripheral.ko`|是|13,072 / 1,904 / 20 / 2|major 106 `peripheral`、17 个 ioctl、MDIO0/1 访问|活动私有外设控制，A|
|`plat_132.ko`|是|416,984 / 115,636 / 556 / 286|PON/NPPT platform、IRQ、时钟/复位、SerDes、CPU/IDM net path|活动核心平台模块，A/B|
|`rio-scan.ko`|否|17,696 / 5,552 / 21 / 0|RapidIO enumeration/discovery，`scan` 参数|库存，C|
|`rlt8226b.ko`|是|73,448 / 23,540 / 89 / 30|RTL8226B 2.5G PHY、MDIO、热传感、固件路径|活动 PHY 模块，A/B|
|`rtl8373_switch.ko`|是|898,960 / 257,888 / 1,516 / 25|RTL8373/8372 ASIC、ACL/寄存器/统计、MDIO、proc 调试|活动交换机底层，A/B|
|`shellproc.ko`|是|20,608 / 4,936 / 19 / 0|`/proc/tm/shell`、kallsyms/PTE 诊断 shell|活动私有诊断，A|
|`switch.ko`|是|341,312 / 85,480 / 289 / 10|vendor Ethernet netdev、FFE frontend、PHY/TM API、`/proc/laninfo`、sysfs|活动网络控制面，A/B|
|`tsi568.ko`|否|7,368 / 1,052 / 11 / 0|RapidIO Tsi568 switch|库存，C|
|`tsi57x.ko`|否|9,312 / 2,248 / 16 / 0|RapidIO Tsi57x family|库存，C|
|`tsi721_mport.ko`|否|35,840 / 15,056 / 50 / 0|PCI 111d:80ab、RapidIO mport、MSI/MSI-X、DMA/BAR|库存，C|
|`ulpi.ko`|否|10,344 / 1,028 / 16 / 6|ULPI bus 和 OF modalias|库存，C|
|`usb-storage.ko`|否|135,880 / 15,380 / 82 / 25|USB mass storage/SCSI、391 USB aliases、PM/suspend|库存，C|
|`usb_led.ko`|否|7,528 / 476 / 7 / 0|USB LED callback/GPIO abstraction|库存，C|
|`virtio_crypto.ko`|否|27,016 / 7,308 / 50 / 0|virtio crypto device 0x14，依赖 crypto_engine|库存，C|
|`wlan_debug_module.ko`|否|91,112 / 25,284 / 106 / 0|厂商 WLAN procfs/debug hooks|库存，不能证明 WLAN 数据面，C|
|`xdpi.ko`|是|1,420,768 / 464,696 / 2,310 / 2|软件 DPI、Bloom filter、BOTNET/应用数据库、文件系统 DB hooks|活动产品 DPI，A/B|
|`zx279051.ko`|是|50,552 / 16,756 / 30 / 36|ZX279051 PHY/SerDes、MDIO/APB、reset/link mode|活动 WAN PHY 模块，A/B|
|`zx_ponreg.ko`|是|9,488 / 1,124 / 17 / 4|major 222、PON/NPPT/PPS register ioctl|活动私有寄存器工具，A|

### 4.1 `np.ko` 内部职责统计

`np.ko` 的 4,555 个定义函数按符号名做非互斥匹配：

|符号家族|匹配函数数|说明|
|---|---:|---|
|FAST/flow/SE/hash/SDT/ZCAM/IKEY/age|708|快速流表、哈希、ZCAM、老化、表项生命周期|
|TM/QMG/queue/scheduler/RED/meter/policer/shaper/QoS|997|流量管理、队列、调度、整形、拥塞/RED|
|ACL/filter/classifier|54|ACL flow、VLAN translation、过滤/分类|
|PPU/NPPU/parser/SDET/SMCT/SOPC/SIPC/SPA/SMMU|653|解析、分类、处理流水线和内部表|
|IDM/DMA/BMU/BPPE/CPU I/O/reorder|295|CPU packet I/O、ring、buffer pool、回收/重排|
|VLAN/bridge/FDB/multicast/IGMP/MLD|104|二层、VLAN、多播相关接口|
|PON/GPON/EPON/XGPON/OMCI/OAM/DBA/LLID|153|PON 产品和协议接口|
|PTP/PPS/TOD/timestamp/1588|172|时间、1PPS/TOD 和时间戳控制接口|
|PHY/MAC/XMAC/SMAC/SerDes/XPCS/MDIO|475|MAC/PCS/PHY/SerDes 控制|
|stat/counter/cnt|575|统计和计数器接口|
|power/clock/reset/suspend/resume|315|低功耗、clock/reset、频率/电源控制|
|debug/test/dump/print/show/help|962|大量原厂调试和测试接口|

这张表解释了为什么主线直接实现 Linux 标准 flowtable 后端时，代码量会远小于 `np.ko`：主线没有复刻原厂整个产品 SDK、诊断面和私有控制面。但它也直接证明，若目标是“完整原厂能力等价”，当前主线确实还缺大量功能。

## 5. 当前主线内核有哪些能力

### 5.1 通用内核配置

|能力|当前主线状态|与原厂相比|
|---|---|---|
|SMP|`y`，DTS 描述 2 个 Cortex-A53|具备；`NR_CPUS=512` 是通用上限，不代表板上 512 核|
|调度|250 Hz、non-preempt、high-resolution timers|策略不同|
|模块|支持加载/卸载；只有 2 个实际 `m`|具备，模块化范围大幅缩小|
|BPF syscall/JIT|启用|主线新增|
|XDP sockets|启用|主线新增|
|cgroup/namespace/seccomp|当前诊断配置未启用|原厂有，主线当前镜像缺|
|perf/kexec|当前诊断配置未启用|原厂有，主线当前镜像缺|
|suspend/cpuidle/cpufreq/regulator|未启用|原厂配置声明更多，但原厂 DVFS/suspend 也缺乏有效运行证据|
|crypto core|`CONFIG_CRYPTO=n`，只保留少量内部 SHA256 library|明显少于原厂|

### 5.2 SoC、板级控制器和外设

|能力|实现/配置|验证与限制|等级|
|---|---|---|---|
|TOPCRM/LSP clock+reset|ZX279133 CCF/reset provider 内建|串口、MDIO、SFC、WDT、网络均依赖；整机启动证明关键路径工作|A|
|PL011 UART|内建，board `okay`|持续作为串口 console 使用|A|
|GPIO+IRQ|5 个 bank，gpiolib cdev/irqchip|RTL8372N/ZX279051 reset GPIO 的工作间接证明控制器；LED/按键未单独验收|A/B|
|pinctrl|内建|SFC group 和 GPIO ranges 建模；缺完整板上 pin 功能验收|B|
|白/红 LED、reset key|DTS + generic gpio-leds/gpio-keys|代码/配置存在；无保存的交互验收|B|
|PVT hwmon/thermal|内建，提供 `temp1_input`/`in1_input`、OF thermal sensor|缺当前板上读数和 trip/cooling 验收|B|
|SFC/SPI-NAND|内建，board 25 MHz、flash read-only|驱动和 DT 存在；当前配置没有 `MTD_BLOCK/JFFS2`，不能作为原厂持久根文件系统|B/部分|
|eFuse NVMEM|内建，0x80-byte read-only shadow、ttrim cell|代码/DT 存在；缺当前板上读取验收|B|
|watchdog 0/1|内建、nowayout；init 同时启动两个 feeder 并检查 sysfs `state=active`|持续用于诊断镜像|A|
|USB xHCI|内建，USB2/USB3 host 和 mass storage|缺当前板上外设传输验收；没有原厂 DWC3 gadget/USB serial/printer/USB net 广度|B/部分|
|I2C0/1|`CONFIG_I2C=n`，没有当前生产驱动路径|缺失；原厂 adapter 曾 probe 成功但 pin 获取失败|
|PWM|驱动源码存在，当前 `CONFIG_PWM=n`、DTS node disabled|当前镜像不可用；属于配置/启用缺口|
|PCIe|没有当前生产 DTS/driver 路径|缺失；原厂运行快照也未实例化|
|RapidIO|未启用|缺失；原厂也只是库存模块、未加载|
|WLAN|cfg80211/rfkill/无线驱动未启用|缺失；原厂有 integration 簇，但采集快照未证明无线数据面|

### 5.3 主线 WAN/LAN 基础网卡能力

|能力|当前实现|证据/限制|等级|
|---|---|---|---|
|RX/TX ring|IDM，TX 1024、RX 2048；32-bit DMA|源码、硬件寄存器恢复和实际收发|A/B|
|RX 内存|page-pool、DMA_BIDIRECTIONAL、NAPI|实际线速转发依赖此路径|A|
|TX completion/BQL/watchdog recovery|已实现|源码 + 长时间流量/生命周期验收|A/B|
|phylink/MDIO/XPCS/SerDes|已实现|2.5G link、冷启动 direct `bootm`、链路重协商有板上证据|A|
|速率|MAC 声明 10/100/1000/2500；2.5G 与 1G fallback 有证据|10/100 没有同等板上验收|A/B|
|MTU|64..1970|1970 是已验证上限；不是 9K jumbo|A|
|TX checksum|只对非封装 IPv4 TCP 硬件生成；其他协议软件 fallback|源码明确限制；无 UDP/IPv6/encap HW checksum|A/B/部分|
|RX checksum|没有 `NETIF_F_RXCSUM`|缺失|
|SG/TSO/GSO/LRO|未声明、无实现|缺失|
|RSS/多队列|只有 queue 0 的 Linux RX/XDP/XSK 模型；无 RSS|缺失|
|固定 ring 查询|`ethtool -g` 可读 RX 2048/TX 1024|没有 `set_ringparam`|B/部分|
|统计|标准 stats64 + 64 项左右 ethtool SW/HW/XDP/XSK stats|实现；硬件 MIB/软件错误统计可读|A/B|
|动态 RX 地址过滤|明确没有 `ndo_set_rx_mode`；NPPT 固定 admission 会送 foreign unicast/multicast|缺失，不应宣称硬件过滤|
|暂停帧|故意不广告 pause；开启 RX_FLOW 会造成转发限速/丢包|与原厂寄存器行为和 A/B 吞吐试验一致|A|
|EEE|`get_eee/set_eee`，ZX279051 + XPCS + XMAC 联动|曾板上显示 `enabled - active`，1G/2.5G advertised；结果未单独固化为仓库日志|A-/B+|
|MII ioctl|经 phylink|源码实现|B|
|DSA register dump|RTL8372N `get_regs_len/get_regs`；chip/link、ACL template/selector/logging counter、按 ingress port latch 的 HSB|标准 `ETHTOOL_GREGS` 已读回 counter 和 IPv6 parser snapshot|A/B|
|WoL/self-test/cable diagnostics|RTL8372N 专用 offline self-test + PHY cable-test；无 WoL|芯片/PHY 访问和 64/1518 字节 MAC loopback 已板测 PASS，测试后数据面恢复；WoL 缺失|
|system suspend/resume|网卡无 system-sleep callbacks，当前内核 `CONFIG_SUSPEND=n`|缺失|

IDM netdev 的 ethtool ops 仍只有 driver info、link、ts info、link settings、nway reset、pause、ring 查询、EEE、stats/string/count；没有 coalesce、channels、RSS/rxnfc、ring 设置、WoL、self-test 或 EEPROM。RTL8372N DSA user port 另有标准 register dump，用于读取 switch/ACL/HSB 硬件状态。

### 5.4 XDP 与 AF_XDP

当前 netdev 声明：

- `NETDEV_XDP_ACT_BASIC`
- `NETDEV_XDP_ACT_REDIRECT`
- `NETDEV_XDP_ACT_NDO_XMIT`
- `NETDEV_XDP_ACT_XSK_ZEROCOPY`

驱动实现 `ndo_bpf`、`ndo_xdp_xmit` 和 `ndo_xsk_wakeup`；RX 支持 XDP_PASS/DROP/TX/REDIRECT/ABORTED，AF_XDP 有 fill/RX/TX/completion/need-wakeup 生命周期和独立统计。

明确限制：

- 只支持 queue 0；非零 `queue_id` 返回 `-EINVAL`。
- XDP frame 或 XSK buffer 带 fragments/multi-buffer 时返回 `-EOPNOTSUPP`。
- 仍受 32-bit DMA 地址限制。
- 仓库包含 `xsk-zc` 验收程序和题为 “AF_XDP and hardware clock acceptance” 的提交，但没有把最终数值输出固化为独立报告。因此本项可确认“接口和驱动实现完整到可运行夹具”，但不把吞吐、包数和长期稳定性列为 A 级。

证据等级：B+。

### 5.5 PTP/PHC、软件时间戳和硬件时间戳

SoC `zx279133-tod` 实现注册 `/dev/ptpN`，支持：

- `gettimex64`
- `settime64`
- 链路 down/up 后重新启动 TOD
- ethtool `phc_index`

板上曾观测到 link down 时读取返回 `ENETDOWN`，link 恢复后 PHC 每秒正常前进，证明生命周期与基础 get/set 可用。

明确没有：

- `adjfreq`/`adjfine`/`adjphase`；`max_adj=0`
- external timestamp、periodic output、PPS pin
- XMAC RX/TX packet hardware timestamp
- `SIOCSHWTSTAMP`/`SIOCGHWTSTAMP`

ethtool 仍会报告内核通用软件时间戳能力；这不能被写成“完整 PTP 硬件时间戳”。

RTL8372N 自身的 PTP block 另做了真实能力探测：版本寄存器为 `0x201`，
配置频率和 current-frequency 均接受 `0x10000000`，但内部 TOD 不前进；
切换外部时钟源后 apply 超时，说明本板也没有可用的外部参考时钟。驱动会
识别这个状态并拒绝注册一个静止的 PHC，交换机其余功能继续初始化。

结论：SoC 基础 PHC 是 A-/B+；本板 RTL8372N PTP 不可用，完整 packet
hardware timestamp 仍缺失。后续除非取得板级 PTP 时钟连线或 Realtek 对
RTL8372N N 变体的补充资料，否则不再为这个未供时模块继续扩展接口。

### 5.6 RTL8372N DSA 交换机

已实现 DSA ops：

- bridge join/leave
- 原厂式 S-VLAN DSA tagger；RTL ports 4..7 使用私有 SVID59..62，CPU8 为 service port；VLAN filtering/add/del、PVID access、tagged trunk 和 VID 0
- STP state；learning、unicast/multicast/broadcast flood 和 isolated bridge flags
- FDB add/del/dump、ageing time 和 fast-age
- MDB add/del（VID 0 SVL、非零 VID IVL）
- Linux bridge IGMP/MLD snooping 自动生成和删除硬件 dynamic MDB
- LAG join/leave 和 layer2/layer2+3/layer3+4 hash
- LAG active member change、静态 FDB 迁移、链路 down/up、inactive member leave/rejoin、删除复用和完整 teardown
- tc matchall port mirror 和 ingress policer
- tc flower template 0..4：L2 EtherType/MAC、IPv4、完整 IPv6、C/S/QinQ VLAN、L4 exact/masked port 和 16 项 range table，pass/drop/mirror/redirect/trap、`skbedit priority`、IPv4/IPv6 DSCP pedit remark、C-VLAN push/pop/modify
- tc flower per-rule shared meter police；meter replace/delete/reuse
- tc flower delayed hardware packet stats/`lastused`；32 项 logging counter 的删除复用
- DCB default priority、全局 DSCP map、PCP/DSCP apptrust
- tc ETS 8 队列 strict/WFQ 和 priority→queue map；root/queue TBF
- per-port MTU/max MTU
- phylink caps/fixed state
- ethtool MIB、标准 pause stats 和 switch/ACL counter/HSB register dump
- 专用 ethtool offline self-test：芯片/PHY 访问和 64/1518 字节 MAC local-loopback

板上已经验证 VLAN-unaware bridge，以及 PVID 100 access + tagged trunk 的
VLAN-aware 模型。四个用户口映射为 `lan1`..`lan4`；专用 tagger 只移除
SVID59..62 外层 transport tag，保留内层客户 C-VLAN。MDB、mirror、bridge flags、policer 和 flower
均有真实硬件数据面 A/B，不只是控制面回调成功。

当前仍缺：devlink health/params、EEPROM、CBS/TAPRIO/ETF、per-queue stats
和更完整的 storm-control 接口。ACL 只有
32 个 logging counter，采用 32-bit packet 模式，没有同步的 byte counter；
logging 与 per-rule police 共享 action selector，不能同时启用。IPv6、L4 range
和单 C-VLAN flower 均有真实 packet A/B；CPU trap、C-VLAN push 和 ID/PCP
modify 也已真实命中。C-VLAN pop 已有硬件 packet-hit，但还需带 VLAN100 CPU
membership 的 tagged fixture 完成端点 A/B。
LAG 的单 active member 转发、故障切换和完整生命周期已经验收；双在线
成员的按 hash 分流、MDB LAN-to-LAN 复制和 mirror 目标副本仍需第二个
在线 LAN 端点完成最终验收。

结论：基础 DSA 和多项高级交换功能已进入可用、可验收状态，但还不是
原厂 `rtl8373_switch.ko` 的全部产品功能。

### 5.7 IPv4/IPv6/PPPoE 硬件 flow offload

主线绕过原厂 `switch.ko` 私有 FFE frontend，直接把标准 `TC_SETUP_FT`/flower 规则编译成 NPPT fast entry。

已支持：

- 精确 IPv4 TCP/UDP 五元组
- 精确 IPv6 TCP/UDP 五元组压缩 key
- LAN↔WAN route
- Ethernet rewrite
- IPv4 SNAT/DNAT、TCP/UDP port NAT、checksum action
- PPPoE push/pop，单 SID
- add/delete/flush/rebind
- 每方向 stats、`lastused`、age
- ZCAM 四块 × 五 cell 的冲突回退
- SDT14 16 MiB DDR bulk overflow
- nftables flowtable 与 clsact flower frontend

关键板上验收：

|项目|结果|
|---|---|
|IPv4 nft flowtable UDP NAT|30 秒 2.459986/2.459971 Gbit/s，包差 0.00056%|
|70 秒 UDP NAT|14,217,379 发 / 14,216,465 收，包差 0.00643%；5/30/60 秒均 `[HW_OFFLOAD]`|
|IPv6 route LAN→WAN|2 GiB，2.340/2.336 Gbit/s|
|IPv6 route WAN→LAN|2 GiB，2.262/2.260 Gbit/s|
|IPv6 硬件命中|1 GiB 中 router busy +1、idle +12,350、softirq 不增长|
|IPv6 over PPPoE|TCP/UDP 双向 `[HW_OFFLOAD]`；WAN 抓包 `0x8864`、SID 1、PPP `0x0057`，无裸 `0x86dd`|
|SDT43 容量|2,048 双向连接/4,096 directions；第 2,049 个原子失败；全部清理|
|SDT14 DDR 容量|4,300 PPPoE 双向连接/8,600 directions；256 ZCAM、8,277 DDR comparator hit、67 ZCAM collision stash|
|DDR 生命周期|高位 age bit、SDT29 counter、bucket 清理、相同 bucket 复用均验证|
|flowtable flush|硬件标志立即移除，软件连接仍转发；重建后新连接可再次 offload|

当前硬件后端明确不支持：

- ICMP/ICMPv6 offload（软件转发已验证）
- 非 TCP/UDP L4 协议
- IP fragments
- IPv6 地址/端口改写、NAT66
- GRE、IPIP、VXLAN、Geneve 等 tunnel/encapsulation
- IPsec/XFRM offload
- multicast/IGMP/MLD flow offload
- 完整 L2 bridge flowtable offload
- 复杂 tagged trunk VLAN actions
- 多 WAN、多 PPPoE、多策略路由
- 多个并行 SNAT 公网源地址：WANID0 只允许一个活动源 IPv4
- 多个并行 PPPoE SID：当前只允许一个活动 SID/context

容量不能只写成一个数字：

- SDT43 compact flow 仍受 4,096 IKEY/age entry 限制。
- SDT14 full flow 可进入 262,144 个 64-byte DDR buckets；已经实测 8,600 directions，不代表已经实测到 262,144 上限。
- 精确 SDT29 packet/byte counter ID 只有 1,024 个；其他 directions 用 age 保持真实 `lastused`，不是每条都有精确包/字节计数。

## 6. 当前主线两个可加载模块

|模块|文件字节|`.text`|定义函数|职责|状态|
|---|---:|---:|---:|---|---|
|`zx279133-rtl8372n.ko`|71,616|37,668|80|RTL8372N DSA switch driver|生产模块，按需加载|
|`tag_zx279133_rtl8372n.ko`|构建时生成|—|2|SR1010 RTL8372N S-VLAN CPU-link tagger|生产模块，RTL 模块依赖|

其余 98 个 `modules.builtin` 条目已经编入 vmlinux，不能再用 `modprobe` 独立卸载。

## 7. 原厂与主线逐子系统差距矩阵

|子系统|原厂活动能力|当前主线|状态判断|
|---|---|---|---|
|CPU/SMP/基础启动|双 A53、SMP|双 A53、SMP|已具备|
|clock/reset/UART/GPIO/WDT|活动|标准 framework 驱动；关键路径实测|已具备，主线建模更标准|
|pinctrl/PVT/eFuse|活动或 probe|驱动/DT 有；部分缺板上读数|部分具备|
|I2C|两个 adapter probe|未启用|缺失|
|PWM/DVFS/regulator|原厂配置存在但 regulator/DVFS 运行失败|PWM 源码有但镜像禁用；DVFS/regulator 无|两边都未形成可信完整 DVFS；主线当前更少|
|SPI-NAND/MTD/JFFS2 产品根fs|原厂完整运行|只读 SFC/SPI-NAND 代码路径；无 MTD block/JFFS2|产品存储能力缺失|
|USB host|xHCI 活动|xHCI+storage 编入，缺板上外设验收|部分具备|
|USB gadget/serial/printer/net|原厂配置广|主线未启用|缺失|
|WAN PHY/SerDes/XPCS|私有模块|标准 phylib/phylink/Generic PHY/XPCS|核心路径已具备|
|LAN switch|私有 RTL SDK，ACL/统计/调试广|DSA bridge/STP/VLAN/FDB/MDB/LAG/mirror/ACL/QoS/MIB|核心与多项高级能力具备，仍非产品全功能|
|WAN/LAN CPU packet I/O|plat/np/switch 私有路径|IDM DMA/NAPI/page-pool/BQL|核心已具备|
|通用 NIC offloads|原厂私有能力较广但 Linux 标准暴露不完整|TX IPv4/TCP csum + XDP/AF_XDP；无 RXCSUM/SG/TSO/RSS|部分具备|
|IPv4 route/NAT offload|FFE/NP FAST|标准 nft/tc → NPPT|已具备并有强验收|
|IPv6 route offload|原厂符号/API 存在|精确 TCP/UDP route|已具备并有强验收|
|PPPoE offload|原厂 FAST/WANID|IPv4/IPv6 push/pop|已具备；单 SID|
|大流表|原厂 ZCAM+DDR/链迁移 SDK|ZCAM + 一个 SDT14 DDR bulk|已具备部分；未复刻所有 hash chain/migration/多 bulk|
|统计/老化|原厂广泛统计|1,024 精确方向计数 + 262K age model|核心 flow 生命周期已具备，广度较少|
|QoS/TM/RED/scheduler|原厂 `np.ko` 约 997 个相关函数|NPPT 固定 TM；RTL8372N 标准 DCB、ETS strict/WFQ、root/queue TBF|交换机基础 QoS 已具备；NP 产品 TM 仍缺|
|ACL/meter/policer|原厂 NP+RTL+FFE|RTL flower L2/IPv4/IPv6/VLAN/range、pass/drop/mirror/redirect/trap/priority、DSCP 与 C-VLAN push/pop/modify、32 项 packet logging counter、port policer、per-rule shared meter|五个模板、CPU trap、push/modify 数据面、标准 `tc -s` packet/lastused 均已接通；pop 还缺 tagged endpoint A/B|
|multicast/IGMP/MLD|原厂内核/NP/交换机支持|Linux bridge IGMP/MLD → dynamic DSA MDB 已验收；无 routed multicast|标准 L2 snooping/offload 已具备；路由多播缺失|
|DPI/应用识别/URL/DNS filter|原厂 xdpi + 内核扩展活动|无|缺失|
|XFRM/IPsec/crypto|原厂 core 启用；硬件 ipsec module 未加载|当前配置关闭|软件产品能力缺失；不能把未加载 vendor module 算活动优势|
|PON MAC/OMCI/OAM/DBA|原厂平台/NP 接口丰富|只有 PON SerDes/route 物理支撑|重大产品栈缺口|
|WLAN integration|原厂核心有集成代码，调试模块未加载|无 cfg80211/WLAN driver|主线缺；原厂实机数据面也未由本快照证实|
|PTP/时间戳|原厂 NP/plat/RTL 有大量私有接口，原厂内核关闭 `CONFIG_PTP_1588_CLOCK`|SoC 基础 PHC；RTL block v0x201 未供时并由驱动禁用；无 packet HW timestamp|部分具备；双方不能宣称完整 PTP|
|低功耗/suspend/WoL|原厂声明多、运行未验证|当前关闭/缺实现|缺失，原厂也缺可靠验收|
|私有诊断/proc/sysfs/ioctl|原厂非常丰富|主线刻意不复刻|按 upstream 设计属于有意缺失；产品维护工具不等价|

## 8. 哪些“缺失”只是当前诊断配置问题

以下多数不需要逆向硬件，先扩展内核配置和 initramfs 就能恢复通用 Linux 能力：

- cgroup、namespace、seccomp
- perf events、kexec
- TUN/TAP
- XFRM、ESP/AH、软件 IPsec
- crypto core 和常用算法
- JFFS2/SquashFS/UBIFS/FUSE/overlay/NTFS3（其中 NTFS3 是否需要应由产品需求决定）
- USB serial/printer/net 等通用 class drivers
- I2C core（但 ZX279133 I2C controller 驱动仍需主线适配）

以下不是单改 CONFIG 可以解决，仍需驱动/硬件契约工作：

- GPON/EPON/OMCI/OAM/DBA
- 原厂 FFE/CSPKernel/DPI/URL filter 产品功能
- 剩余 QoS/ACL/multicast 产品能力
- 多 WAN、多 PPPoE、多 SNAT context
- RX checksum、SG/TSO/RSS、多队列、完整 PTP packet timestamp；本板 RTL8372N PTP block 未供时
- RTL8372N 剩余 per-queue stats、devlink，以及 ACL VLAN pop 的 tagged endpoint A/B
- I2C controller、PWM 实机启用与验收
- suspend/resume/WoL 与完整电源域/DVFS

## 9. 当前不能诚实宣称的能力

1. 不能说“主线已完整替代原厂内核”。
2. 不能说“主线网卡功能完整”；它是高性能路由数据面，不是完整通用 2.5G NIC feature set。
3. 不能说“PTP 硬件时间戳完整”；目前只有 SoC 基础 PHC，本板 RTL8372N PTP block 已确认不走时并禁用。
4. 不能把原厂目录中未加载的 `ipsec`、RapidIO、usb-storage、virtio-crypto、WLAN debug 模块当作原厂实机活动能力。
5. 不能把 SDT14 的 262,144 bucket 设计容量写成已实测 262,144 条；当前实测是 8,600 directions。
6. 不能把 `[HW_OFFLOAD]` 单独当成硬件 packet-hit 证据；本报告只在同时存在 counter/CPU/端点/吞吐证据时写成强验收。
7. 不能说“原厂 suspend/DVFS 完整可用”；原厂运行快照同样缺成功 suspend 和有效 regulator/cpufreq 证据。

## 10. 尚需补的验收

按可信度优先级：

1. 保存 AF_XDP 最终 `rx/tx/completions/need_wakeup` 数值日志，并做 attach/detach、link down/up、MTU 变化、压力和资源回收。
2. 保存 SoC PTP get/set、link down/up 后重启和长时间漂移结果；RTL8372N 只有在取得可用参考时钟或芯片补充资料后才重新开启开发。
3. 对 `lan2`、`lan3`、`lan4` 做与 `lan1` 相同的 link、VLAN、bridge、NAT、offload、压力验收。
4. 在 RAM-only 前提下验收 SFC/SPI-NAND 只读 probe/partition/read；不得写 NAND。
5. 读取并记录 PVT hwmon/thermal，验证 ttrim；不做危险温控试验。
6. 验收 USB host + mass-storage 的枚举和读测试。
7. 验收 LED 和 reset button 的 GPIO/input 事件；避免触发破坏性恢复动作。
8. 若目标从诊断内核升级为产品内核，先定义必须恢复的通用 Linux 配置，再处理 PON、QoS、DPI 等真正驱动缺口。

## 11. 主要证据入口

- 原厂核心匹配总报告：[FINAL_REPORT.md](/Volumes/code/sr1010-kernel/analysis/vendor-kernel-match/results-final/FINAL_REPORT.md)
- 原厂配置：[autokernelconf.normalized](/Volumes/code/sr1010-kernel/analysis/vendor-kernel-match/config/autokernelconf.normalized)
- 原厂分析验证：[validation.json](/Volumes/code/sr1010-kernel/analysis/vendor-kernel-match/results-final/validation.json)
- 原厂工具链/输入：[provenance.md](/Volumes/code/sr1010-kernel/analysis/vendor-kernel-match/results-final/provenance.md)
- 原厂运行硬件事实：[runtime-evidence.md](/Volumes/code/zx279133/vendor-reference/hardware-truth/runtime-evidence.md)
- 原厂模块逐项证据：[vendor-modules.md](/Volumes/code/zx279133/vendor-reference/hardware-truth/vendor-modules.md)
- 原厂/主线 DT 差异：[device-tree.md](/Volumes/code/zx279133/vendor-reference/hardware-truth/device-tree.md)
- 当前主线实际配置：[.config](/Volumes/code/zx279133/out/kernel/.config)
- 当前主线板级映射：[mainline-drivers-bindings.md](/Volumes/code/zx279133/vendor-reference/hardware-truth/mainline-drivers-bindings.md)
- NPPT/flow 验收：[NPPT_FORWARDING.md](/Volumes/code/zx279133/docs/mainline-network/NPPT_FORWARDING.md)
- 主线网络核心：[drivers/net/ethernet/zte](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte)
- 当前构建脚本：[build-zxdbg.sh](/Volumes/code/zx279133/port/mainline/build-zxdbg.sh)

## 12. 已发现的文档一致性问题

[NPPT_FORWARDING.md](/Volumes/code/zx279133/docs/mainline-network/NPPT_FORWARDING.md) 前部第 361–362、420–425 行已经说明 PPPoE push/pop 都使用 full SDT14；后部第 763–765 行仍写 reply 使用 compact pop response。这是旧描述残留。当前源码和 2026-08-31 容量验收应作为权威：**push 和 pop 都是 full SDT14；push age-only，pop 可用 SDT29 counter。**
