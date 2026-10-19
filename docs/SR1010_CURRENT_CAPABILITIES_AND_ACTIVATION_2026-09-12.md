# SR1010 当前主线内核能力与启用方式

核对日期：2026-09-12。范围为本仓库 Linux 6.18.38+、当前构建配置与诊断 initramfs。按用户要求，不盘点 SG、TSO。

**核心结论：平台驱动、正常收发、受限 checksum、IPv4 RSS 和多队列会自动工作；桥接、VLAN、硬件 NAT、ACL、限速、QoS 等需要先配置，随后由内核自动维护。当前镜像的启动脚本没有把这些能力组合成一套完整的路由器配置。**

这里的“自动”指在驱动成功绑定、接口启用、相应硬件可用时，不需要另发功能开启命令；不表示设备始终供电、功能永不停止，或所有报文都能走硬件路径。

**核对基线与可信度**

- 外层仓库 HEAD：`ca65efd124f012537c6e74a65855d356ad426463`；内核 HEAD：`b3a74c427d20de510961ab38c0134315d9791ef3`。
- 当前实际构建配置为 [out/kernel/.config](/Volumes/code/zx279133/out/kernel/.config)，release 为 `6.18.38+`。内核和外层仓库存在原有未提交修改，因此本报告包括当前工作树状态，不只描述 HEAD。
- [源启动脚本](/Volumes/code/zx279133/port/mainline/initramfs/init) 与 `out/rootfs/init` 内容一致。
- 本次只做本地配置、源码和已有证据核对，没有连接板子、重启、部署或重新跑网络测试。下文“默认”是当前代码和脚本的默认；“已验收”是已有记录，不能据此断言此刻板上的实时状态。
- 旧文档中“RX checksum/RSS 未实现”等条目已经过时。RSS 以 [2026-09-09 closure](/Volumes/code/zx279133/docs/mainline-network/RSS_CLOSURE_ACCEPTANCE.md) 和当前源码为准；完整硬件验收清单里的“通过条件”也不等于每一项均已通过。

**一、启动后自动生效，或者接口启用后自动工作**

| 能力 | 当前默认行为 | 范围与证据 |
|---|---|---|
| 双核 CPU、SMP、内存管理、IRQ、定时器 | 内核自动初始化和调度 | 双 Cortex-A53、GICv3、ARM timer、DDR 有平台核心板测记录；不需要手工启用第二核 |
| 时钟、复位、pinctrl、UART | 驱动按依赖自动管理；串口 console 可用 | 已有启动及真实 I/O 验收；不等于所有时钟始终开启 |
| GPIO、按键 input、LED 设备注册 | 自动出现对应设备接口 | 按键报告 `KEY_RESTART` 事件，当前 init 没有把它绑定到恢复出厂动作 |
| PVT 温度/电压、thermal zone、eFuse trim | 自动注册；PVT 自动使用有效的 NVMEM trim | 可读温度、电压与 eFuse；温度监测不等于自动调频降温，当前 CPU_FREQ 关闭 |
| 双看门狗 | init 自动启动两个 feeder | 两个设备均使用 10 秒 timeout、2 秒喂狗周期；`WATCHDOG_NOWAYOUT=y`。两路独立超时复位有板测记录 |
| SPI-NAND/MTD | 自动识别并暴露分区 | 当前板级 DTS 标为只读；已有读取一致性验收，无需挂载即可进行 MTD 读取 |
| USB xHCI、USB 存储设备发现 | 控制器自动初始化，插盘后自动枚举 | 已有 USB3 存储读取与重连证据；不会自动挂载文件系统或启动共享服务 |
| WAN 链路与普通收发 | init 自动配置 `eth0` 为 `192.168.1.1/24` 并启用；PHY/phylink 处理链路 | 驱动具备 10/100/1000/2500 能力描述，具体速率由链路协商；不代表全部端口/速率组合都已验收 |
| IDM DMA、IRQ/NAPI、page-pool、TX completion、BQL | 接口运行时自动执行 | 属于普通数据通路机制，不用逐项配置；统计和异常恢复也由驱动处理 |
| GRO | 普通内核收包路径默认允许软件聚合 | 可通过 ethtool 调整；这是软件聚合，不能据此声称硬件聚合 |
| TX checksum | 默认允许硬件生成校验和 | 当前真正硬件路径限非封装 IPv4/TCP；UDP、IPv6等不满足条件的包使用软件回退。不能只看 `tx-checksumming: on` 判断覆盖范围 |
| RX checksum | 默认启用硬件校验结果信任 | 基本未分片 IPv4/IPv6 TCP/UDP、受支持的单层 VLAN 有验收；options、扩展头、复杂封装、零 UDP checksum 和执行过 XDP 的包有保守回退 |
| IPv4 RSS 与 RX hash 上报 | 驱动默认 `rx_hash=true`，加载并验证受限 PPU 哈希程序；RXHASH feature 默认开启 | 支持受保护的基本 IPv4 TCP/UDP；不支持 IPv6 硬件 RSS。固件验证失败时有回退路径，不能仅凭 feature 位确认活动状态 |
| RX/TX 多队列 | WAN `eth0` 与 LAN conduit `lan-cpu0` 各建立 2 RX / 2 TX 队列 | 默认创建，无需 `ethtool -L`；DSA 的 `lan1..lan4` 不是四套独立 RSS 引擎。每个业务不保证平均使用所有队列 |
| RX IRQ 初始 CPU 分配 | 驱动将两路 RX worker 的初始中断亲和性分散到在线 CPU | init 保留该分配；用户后续可以覆盖。无需先配置 RPS 才能获得现有硬件分流 |
| 硬件 TC 接口 | `hw-tc-offload` feature 默认开启 | 只表示驱动允许接收硬件规则；没有配置规则时，不会凭空产生 ACL、限速或 NAT |
| IPv4 转发开关 | init 明确写入 `net.ipv4.ip_forward=1` | **该开关已经自动开启**；但 LAN 地址、出口路由、NAT、过滤规则仍需配置 |
| SoC 基础 PHC | 驱动初始化时注册基础时间接口 | 具备读时/设时；不是完整包硬件时间戳，也不会自动运行 PTP 时间同步服务 |
| bootinfo 只读接口 | 当前工作树和 `.config` 已加入；存在有效 `/chosen/versioninfo` 时自动生成 sysfs 属性 | 属于当前未提交修改；本次未验收运行。无有效记录时目录缺失是正常情况，不能用于证明当前 RAM 镜像来自哪个 flash 槽位 |

默认 feature 位见 [zx279133-main.c:509](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-main.c:509)，初始 IRQ/RSS map 见 [同文件:628](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-main.c:628)，双队列常量见 [zx279133-lan.h:11](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-lan.h:11)。平台与外设的已测范围见 [平台结果](/Volumes/code/zx279133/out/acceptance/20260902-platform-core/RESULT.md) 和 [外设结果](/Volumes/code/zx279133/out/acceptance/20260902-board-peripherals/RESULT.md)。

**二、需要先配置，配置完成后会自动维护**

这类能力通常只需在启动配置中设置一次，之后无需逐包或逐连接人工操作。

| 能力 | 要先做什么 | 配置完成后自动做什么 / 限制 |
|---|---|---|
| 四口 LAN / RTL8372N DSA | 当前交换机驱动和 tag driver 为 `=m`，需加载相应模块，再启用所需端口 | 驱动创建/管理 DSA 端口和 PHY；当前 init 未显式加载模块，也未启动 udev/mdev 自动加载流程，不能把打包了 `.ko` 当成开机已启用 LAN |
| LAN 二层硬件桥接 | 用 `ip link` 建 bridge，把需要互通的 `lanX` 加入并启用 | DSA/switchdev 下发硬件成员关系；在允许学习的状态下自动学习动态 FDB 并老化。不会在当前脚本中自动把四个 LAN 合成一座桥 |
| VLAN / PVID / tagged trunk | 启用 bridge VLAN filtering，配置端口 VID、PVID、untagged/tagged | 内核同步到交换机；硬件执行对应 VLAN 隔离和标签动作 |
| STP / MST、bridge flags | 配置 STP/MSTI、端口状态、learning、flood、isolated 等；完整协议控制按方案配用户态服务 | 驱动同步状态；STP 不能仅因驱动有回调就视为正在运行 |
| FDB 静态项、ageing 参数 | 用 `bridge fdb` 和 bridge 属性配置 | 静态项下发；动态学习/老化在启用后自动维护；无需逐个手工添加普通动态 MAC |
| IGMP/MLD snooping 与 MDB | 先搭建 LAN bridge，按网络需要设置 snooping、querier；静态组用 `bridge mdb` | Linux 处理成员报告，驱动自动同步组播成员到硬件；与跨接口三层组播路由不同 |
| LAG / bonding | 创建受支持的 bond 并加入端口；已验证重点是 `balance-xor` 的三类 hash 策略 | 驱动跟随成员、链路 active 状态和 FDB 变化；不能扩大为任意 bonding/LACP 模式均已验收 |
| IPv4 软件路由 | 配 LAN/出口地址、路由与必要的邻居可达条件 | 转发开关已由 init 打开；没有 NAT 需求时无需强制配置 NAT |
| IPv6 软件路由 | 配 IPv6 地址、路由和 IPv6 forwarding；按需求配置 RA/DHCPv6 服务 | 当前 init 没有打开 IPv6 forwarding；有 IPv6 协议栈不代表已经提供 IPv6 路由服务 |
| IPv4 NAT / SNAT / DNAT | 用 nftables 配相应 NAT 规则 | conntrack 自动跟踪连接并应用转换；当前可用固定地址 SNAT/DNAT，`masquerade` 未编入 |
| NPPT 硬件路由/NAT 快路径 | 配 nft flowtable、`flags offload`、正确的 ingress devices，以及让合适连接进入 flowtable 的规则；也可使用受支持的 tc flower 前端 | 合适的新建 TCP/UDP 连接建立后可自动卸载，维护硬件计数、lastused、老化和删除；不是开机即有自动硬件 NAT |
| IPv6 / PPPoE 硬件流卸载 | 先建立对应路由或 PPPoE 会话，再配置适用的 flowtable/规则 | 已有 IPv6 路由及 IPv4/IPv6 PPPoE push/pop 的受限路径证据；不能扩成 NAT66、任意封装、分片或任意多 WAN 拓扑 |
| PPPoE 拨号 | 配 pppd、对端、认证与网络参数并启动 | 内核提供 PPP/PPPoE 数据路径；rootfs 有 pppd 文件，但 init 没启动拨号 |
| NPPT 表分配、碰撞回退、DDR overflow | 先有硬件流规则；普通使用不需要手工写 SDT/DDR 寄存器 | 由驱动选表、分配、回收；表容量、DDR 直接命中与特定规模测试应分别看证据，不能从预留内存推算已验收连接数 |
| RTL8372N ACL / tc flower | 在对应 LAN 端口安装受支持的 `tc` 规则 | 硬件按匹配执行 pass/drop/trap/mirror/redirect、优先级、DSCP、C-VLAN 等动作；不支持的组合需按返回错误/软件回退处理 |
| 端口镜像 | `tc matchall` 或受支持的 flower + `mirred` | 匹配时硬件复制；开机没有自动镜像策略 |
| 端口限速 / ACL meter | `tc matchall police`、flower police 等 | 硬件执行约定的速率/突发限制；有 A/B 限速证据，不代表任意动作组合都可卸载 |
| LAN 交换机 QoS | 配 DCB priority/DSCP/PCP 信任、ETS 队列/权重、root/queue TBF | 硬件按策略分类、排队或整形；这套 RTL LAN 能力不能当作 WAN/NPPT 硬件 QoS |
| 软件 TBF / ETS | 安装相应 qdisc | 当前构建已提供；硬件卸载与否取决于设备和对应驱动支持，不能仅凭 qdisc 创建成功判断 |
| RPS / RFS / XPS 软件分流 | 按需求配置队列 CPU mask、流表等 | `.config` 已启用相关机制；init 没有配置这些策略。编译开启不等于正在使用，不等于硬件 RSS |

交换机接口依据 [DSA ops](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-rtl8372n.c:7538) 与已有验收材料。镜像目的端实际收到副本、LAG 双在线成员分流、MDB 多端点复制、ETS 多类并发权重等场景仍有证据边界，不能把接口支持写成全面验收。

现有 [nft flowtable 夹具](/Volumes/code/zx279133/port/mainline/nat-acceptance/nft-flowtable.nft) 展示了所需结构，不过它是固定测试地址和端口的验收脚本，首行会 `flush ruleset`；应按实际网络整合规则，不应当作通用开机配置直接覆盖已有规则。判断实际硬件加速需要结合 conntrack 标志、硬件命中计数和端点流量，不能只看 `in_hw` 或 `[HW_OFFLOAD]`。

**三、需要主动调用或按需求调整**

| 能力 | 接口 / 操作入口 | 当前行为与边界 |
|---|---|---|
| RSS 分配表调整 | `ethtool -x DEV` 查询，`ethtool -X DEV ...` 设置 | WAN/LAN 各 8 bucket，值只能选 RX queue 0/1；默认 `0 0 0 0 1 1 1 1`。`equal 2` 会改成交替排列，并不是恢复该初始表 |
| RX hash 上报开关 | `ethtool -K DEV rxhash on/off` | 控制 skb 是否使用驱动提供的 hash；不能把关掉上报等同于关掉 PPU 队列分流。驱动 `rx_hash` 参数是只读启动参数，默认 true |
| checksum / GRO 开关 | `ethtool -k` 查看，`ethtool -K` 修改设备暴露的项 | RXCSUM off 停止协议栈信任标记，但当前设计保留 MAC 的错误校验过滤；不等于保证坏 checksum 帧能送到 CPU |
| MTU / 链路速率 / pause / EEE | `ip link`、对应设备的 ethtool 接口 | 标准能力按设备区分。EEE enabled 与实际 negotiated/active 不是一回事，不能标成始终处于节能状态 |
| ring / channel 诊断 | `ethtool -g`、`ethtool -l` | 驱动有查询能力；WAN 没有对应 set_ringparam/set_channels/coalesce 配置回调，不支持任意调 ring 大小、队列数或中断合并 |
| XDP | 加载并挂载 BPF 程序到支持的网卡入口 | 有 PASS/DROP/TX/REDIRECT 等路径；内核支持不代表开机自动挂载业务程序 |
| AF_XDP zero-copy | 用户程序创建 socket/UMEM 并绑定支持的入口 | 当前 queue 0；共享 UMEM 会改变有效 RX 分流并引起共享引擎生命周期切换，退出后恢复普通路径 |
| PHY cable test / 交换机 offline self-test | 对应 LAN 端口的 ethtool 诊断命令 | 主动诊断，不是后台长期任务；有独立自测和线缆诊断记录 |
| PHC 读写 | 标准 PHC 工具或 clock API | SoC 仅基础读写；没有自动时间同步。RTL8372N 当前板上计时器不走，驱动禁止注册不可用 PHC |
| LED 灯态 | `/sys/class/leds/.../brightness` | DTS 默认 `keep`，保留此前状态；**本构建 `LEDS_TRIGGERS=n`，不能运行时开启 heartbeat/timer trigger** |
| 按键业务 | 用户程序读取 `/dev/input/event*` | 内核报告按下/松开；重启、恢复出厂、长按动作需要用户态逻辑 |
| GPIO、温度、电压、eFuse 读取 | GPIO character device、hwmon、thermal、NVMEM | 设备自动注册，业务采集、告警、灯态联动需主动实现；不能据此声称已配置温控闭环 |
| USB 文件系统使用 | 对已有文件系统执行 mount，随后文件操作 | EXT4、VFAT、exFAT 已编入；无自动挂载/NAS 服务。已有 USB 读取验收不覆盖文件系统写入，原写入测试被未格式化测试盘阻塞 |
| 统计与诊断 | `ethtool -S`、`tc -s`、`bridge`、procfs/sysfs/debugfs | 读取现有 RX/TX/队列/hash/规则统计；不会自动生成监控告警或长期报告 |

RSS 配表会暂停/重启共享数据通路，可能引起 WAN 重协商。有活动硬件流或共享 UMEM 时，修改表会被拒绝；相同表重复写入为 no-op。表可跨引擎重载保留，但不自动跨 reboot 持久化。不支持更换 Toeplitz/key/hash fields 或额外 RSS contexts。见 [配置契约](/Volumes/code/zx279133/docs/mainline-network/RSS_CONFIGURATION_ACCEPTANCE.md:17) 和 [当前 setter](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-netdev.c:1395)。

RSS closure 有 166 项正确性检查，包括 payload、skb hash、实际队列和生命周期；历史 LAN 顺序异常在定向复现中未重现，但根因仍未定位。这里保留该限制，不将“功能矩阵通过”表述为“历史所有问题已消除”，也不推导普遍吞吐提升。

**四、当前不能靠运行时配置直接打开**

| 能力 | 当前原因 | 需要的工作 |
|---|---|---|
| IPv4/IPv6 策略路由、多路由表 | `IP_ADVANCED_ROUTER=n`、`IPV6_MULTIPLE_TABLES=n` | 补配置依赖、重编内核，再配路由策略 |
| WireGuard、TUN 用户态 VPN、完整 IPsec | `WIREGUARD=n`、`TUN=n`、`XFRM_USER=n`、ESP 关闭、`CRYPTO=n` | 补内核选项/算法与用户态服务；不能只启动一个 VPN 配置文件 |
| 跨接口 IPv4/IPv6 路由多播 | `IP_MROUTE=n`、`IPV6_MROUTE=n` | 补内核和路由多播控制程序；已有 LAN MDB 不替代该能力 |
| nft masquerade / redirect / reject / limit / log | 对应 `NFT_MASQ/REDIR/REJECT/LIMIT/LOG=n` | 重新编入相应表达式；不是普通 SNAT/DNAT 全部不可用 |
| nft socket / TPROXY、bridge family | `NFT_SOCKET=n`、`NFT_TPROXY=n`、`NF_TABLES_BRIDGE=n` | 补编译配置；DSA bridge 硬件交换仍是另一条可用路径 |
| conntrack netlink 管理/事件接口 | `NF_CT_NETLINK=n` | 目前已有 procfs 查询不能替代完整 netlink 管理接口 |
| CAKE、FQ-CoDel、FQ、HTB | 当前相关 qdisc 均未编入 | 重新编入所需软件队列算法；已有 TBF/ETS 不等于具备 SQM |
| 自动 CPU 调频 / cpuidle / suspend | `CPU_FREQ=n`、`CPU_IDLE=n`、`SUSPEND=n` | 需配置并核对平台实现；现有 thermal 监测不补足这些机制 |
| I2C、PWM、PCIe 的当前板级使用 | 本构建 `I2C=n`、`PWM=n`、`PCI=n` | 需内核配置及板级驱动/DT/硬件验证 |
| Wi-Fi 网络服务、PON 接入业务 | 没有据此确认的可用板级 WLAN / PON MAC 业务链路 | `WLAN=y` 或使用名为 PON SerDes 的以太网链路不能证明 Wi-Fi、OMCI/OAM/DBA 已支持 |
| UBI/UBIFS 持久化根文件系统、overlay | `MTD_UBI=n`、`OVERLAY_FS=n`，当前为诊断 initramfs | 需要单独的存储与系统集成工作 |
| 完整包硬件时间戳 / PTP 功能 | SoC 只提供基础 PHC；RTL8372N 时钟已有不可用证据 | 不能用当前 PHC 读写接口替代 TX/RX timestamp、调频、PPS/extts |

这些构建状态均以本次读取的 `.config` 为准。与源码支持但未使用的能力相比，它们需要重编译，部分还需要驱动或板级实现。

**五、当前镜像启动时究竟做了什么**

[init](/Volumes/code/zx279133/port/mainline/initramfs/init:23) 自动挂载 devtmpfs/proc/sysfs/tmpfs/debugfs，打开 IPv4 转发，启动双 watchdog feeder，启用 loopback，等待 WAN 驱动，然后给 `eth0` 配 `192.168.1.1/24` 并进入串口 shell。最后还把 init 及其后续应用子进程初始 affinity 固定在 CPU0；这与网卡 RX worker 的 CPU 分配是两件事。

脚本没有加载 LAN 交换机模块、创建 bridge/VLAN/bond、配置 LAN 地址或缺省网关、加载 nft/tc 规则、启动 PPPoE/DHCP/DNS/RA、开启 IPv6 转发、挂载数据盘或启动持久化管理服务。本次核对的脚本也没有启动 telnetd/iperf3，旧材料中的临时服务状态不能当作当前启动默认。

因此，当前“自动生效”的主体是内核和网卡基础机制；要把现有能力用于日常路由器，需要补一层可重复执行的模块加载、网络、规则与服务启动配置。配置好以后，FDB 学习、组播成员同步、合适连接的硬件卸载、流量计数和老化都可以自动工作。

**只读核验入口（供上板时使用，本次未执行）**

```sh
uname -r
ip -br link
ip -br addr
ip route
ip -6 route
cat /proc/sys/net/ipv4/ip_forward
cat /proc/sys/net/ipv6/conf/all/forwarding
lsmod
ethtool -k eth0
ethtool -l eth0
ethtool -x eth0
ethtool -S eth0
# 存在 LAN conduit / DSA 端口时再查询：
ethtool -l lan-cpu0
ethtool -x lan-cpu0
bridge link
bridge vlan show
bridge fdb show
bridge mdb show
nft list ruleset
tc qdisc show
```

这些命令依赖完整版本的 iproute2/ethtool/nft；精简 BusyBox 命令不一定支持全部选项。对照结果时要同时看接口是否存在、模块是否加载、规则是否存在和实际计数是否增长，不能单凭内核选项或 feature 位判定业务已经运行。
