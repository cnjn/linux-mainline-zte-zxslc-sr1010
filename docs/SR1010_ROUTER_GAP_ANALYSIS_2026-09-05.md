# SR1010 当前内核离优秀路由器内核的差距

审阅日期：2026-09-05。范围：SR1010 有线以太网路由器；无线、光接入和 NAS 按可选产品方向单列。

**当前已经有很扎实的有线硬件转发基础。接下来最值得补的是：低延迟 QoS、完整的防火墙与动态 WAN 支持、策略路由与 VPN、CPU 软件转发效率，以及硬件加速和真实路由策略之间的正确配合。RTL8372N 的功能数量已经不是首要短板。**

这里的“优秀”采用四个实际标准：常用路由业务完整；满载时交互延迟可控；配置和链路变化后行为正确；性能与故障有可信的观测及恢复手段。没有必要以复刻全部原厂私有功能或填满 ethtool 接口为目标。

## 审阅依据与边界

- 已读取任务 [适配主线内核网络硬件加速 (6)](codex://threads/01a04488-1153-7ae1-bce2-796006f8048e) 的近期结果，包括 PHY、ACL、MST、LAG、自检和 PTP 的收口。
- 已核对当前工作区源码、`out/kernel/.config`、初始化脚本，以及硬件验收、NPPT、ACL、MDB、QoS 文档。
- 外层 HEAD：`639322ad9`；内核 HEAD：`5336656ce`。两者均有未提交变更，本报告包含这些变更，不能把 HEAD 当作当前功能的完整版本标识。
- 当前磁盘 FIT 的实际 SHA-256：`2a8379832c5d1511b150496318ff460468105141e6be1e5c76729c4cd7eafc29`，与指定任务的最终构建记录一致。
- 本次是静态审阅，没有重启或操作板子，没有新跑性能测试。历史板测证明对应版本和场景的能力；本次没有据此宣称最终 FIT 已重跑全部组合场景，也没有确认板上此刻运行的镜像。

状态用语：**配置缺口**表示上游实现已有、当前构建未启用；**驱动缺口**表示当前设备实现缺少该路径；**集成缺口**表示单项存在但组合策略未闭合；**验收缺口**表示实现已有、指定实际场景证据不足。

## 已有能力，不应继续列为“缺失”

|领域|当前可确认的基础|应保留的边界|
|---|---|---|
|平台|双核、时钟/复位、UART、GPIO/LED/按键、PVT、eFuse、看门狗、USB 存储读取等已有驱动和相关板测|USB 文件系统写入未因读测试成功而获得验收；持久化存储仍另论|
|CPU 收发|IDM DMA、IRQ/NAPI、page-pool、GRO、BQL、TX completion/recovery、phylink/PCS/SerDes|普通网卡卸载范围仍有限，见下文|
|路由和 NAT|IPv4/IPv6、TCP/UDP、IPv4 SNAT/DNAT、PPPoE IPv4/IPv6、标准 nft flowtable/flower 到 NPPT|协议和拓扑限制仍在，不能扩大成任意流量均可硬件加速|
|流表生命周期|添加、删除、flush、重绑、老化、精确计数的有限分配、ZCAM 碰撞回退、SDT14 DDR overflow|4,300 双向 PPPoE 连接的证据不等于整个 DDR 地址空间的容量验收|
|交换|DSA bridge、VLAN、STP/MST、FDB、bridge flags、动态 IGMP/MLD→MDB、hash LAG 生命周期|部分多端点数据面仍待补验收|
|交换机 ACL/QoS|IPv4/IPv6/VLAN/range flower、drop/trap/mirror/redirect、DSCP/C-VLAN 动作、meter、packet stats、DCB、ETS、TBF|RTL 交换机 QoS 与 WAN/NPPT QoS 是不同硬件路径|
|诊断及高级接口|内部 PHY/phylib、EEE 配置、cable-test、四项专用 offline self-test、XDP、AF_XDP zero-copy、SoC 基础 PHC|AF_XDP 只支持 queue 0；EEE 配置通过不等于对端支持 LPI；基础 PHC 不等于包硬件时间戳|

源码中的 [DSA ops](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-rtl8372n.c:7539) 与更新后的 [能力审计](/Volumes/code/zx279133/docs/SR1010_KERNEL_CAPABILITY_AUDIT_2026-08-31.md:341) 能对应上述交换功能。指定任务最后建议冻结 RTL8372N 功能扩展，这个方向合理；仍应接收实际 bugfix 和回归修复。

## 1. 满载仍低延迟的 QoS：第一优先级

**缺口性质：配置 + WAN/NPPT 驱动 + 策略集成。**

当前 `CONFIG_NET_SCH_CAKE=n`、`NET_SCH_FQ_CODEL=n`、`NET_SCH_HTB=n`、`NET_SCH_HFSC=n`、`IFB=n`；已启用的是 TBF、ETS 等。上游代码已经提供 CAKE/FQ-CoDel，因此软件 SQM 的第一步主要是配置和用户空间集成，不需要先逆向芯片。[当前 qdisc 配置](/Volumes/code/zx279133/out/kernel/.config:1032)、[上游 Kconfig](/Volumes/code/zx279133/linux-6.18.38/net/sched/Kconfig:277)

RTL8372N 的 8 队列、strict/WFQ、priority mapping、TBF 和 policing 已有实际证据，例如删除软件 TBF 后，硬件 queue 0 仍限制在约 96.2 Mbit/s。但这只证明了对应交换机队列的硬件整形，尚不能回答“家里有人跑满上下载，语音和游戏是否仍流畅”。[QoS 板测](/Volumes/code/zx279133/port/mainline/network-acceptance/RTL8372N_QOS.md:42)

WAN netdev 的 `ndo_setup_tc` 目前直接进入 NPPT flow offload；后者只接收 `TC_SETUP_BLOCK`、`TC_SETUP_FT`，没有可配置 WAN 队列调度/整形的 qdisc 接口。固定 TM 初始化不等于可管理的 WAN QoS。[WAN setup_tc](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-netdev.c:1296)、[NPPT setup_tc](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-offload.c:1271)

应该补到：

1. 一个可以实际使用的 CAKE 或 FQ-CoDel 加整形方案，覆盖上行和下行，正确设置 PPPoE/VLAN 开销。
2. 明确哪些流量需要经过软件排队。需要 SQM 的流不能继续无条件留在 NPPT 硬件快路径。
3. 先测出双核 CPU 能承受的 SQM 吞吐与延迟，再决定是否投入 WAN/NPPT 硬件 shaper、分类和队列接口。
4. 用同时上传、下载和低速交互流验收：吞吐、RTT p95/p99、丢包、CPU/softirq、各主机公平性。只报 Gbit/s 或限速值不够。

这里不能预先保证“CAKE 能跑满 2.5G”，也不能把交换机 TBF 当作 CAKE 的等价实现。

## 2. 日常防火墙与动态 WAN：高收益配置缺口

**缺口性质：配置 + 用户空间策略。**

当前已经有 nftables、conntrack、NAT、flowtable；但实际构建关闭了以下常用组件：

|缺失组件|实际影响|建议|
|---|---|---|
|`NFT_MASQ`|不能直接使用 nft `masquerade` 跟随动态 WAN 地址；当前样例是固定源地址 SNAT|动态 DHCP/PPPoE WAN 优先补|
|`NFT_REDIR`|本机 DNS 等常见重定向表达式不可用|按服务需求补|
|`NFT_REJECT`、`NFT_LIMIT`、`NFT_LOG`|缺少常见拒绝响应、规则限速和日志表达式|作为基础防火墙工具补齐|
|`NF_CT_NETLINK`|标准 conntrack 用户空间管理/事件方案受限，当前主要依赖 procfs|补 netlink；事件、mark、zone、timeout 按需求连同依赖一起启用|
|`NFT_SOCKET`、`NFT_TPROXY`|透明代理路径未具备|需要透明代理时补|
|`NF_TABLES_BRIDGE`|缺少 nft bridge family 规则|需要桥侧软件安全策略时补，不与 DSA VLAN 隔离混为一谈|

证据：[实际 netfilter 配置](/Volumes/code/zx279133/out/kernel/.config:904)。缺失的 Kconfig 符号可能因依赖关闭而没有独立 `# ... is not set` 行，不能只粘贴叶子选项而不解依赖。

[现有 IPv4 验收规则](/Volumes/code/zx279133/port/mainline/nat-acceptance/nft-flowtable.nft:1) 使用固定 `snat ip to 192.168.1.1`，forward policy 为 accept，并执行 `flush ruleset`。这是验收 fixture。产品策略还需要 WAN 入站限制、LAN/访客隔离、必要 ICMP/ICMPv6、端口映射与动态地址更新。

DHCP、DHCPv6-PD、RA、DNS、UPnP/PCP、拨号守护与断线重连主要属于用户空间。不能将未集成这些服务写成“Linux 没有 IPv6/PPPoE”。但优秀路由体验需要这些服务与内核、NAT 及硬件流表共同工作。

## 3. 策略路由、VPN 和可选路由多播

**缺口性质：大量是配置；相应硬件加速另有驱动限制。**

|功能|当前状态|需要补到的程度|
|---|---|---|
|IPv4/IPv6 策略路由、多路由表|`IP_ADVANCED_ROUTER=n`，IPv4 policy-routing 依赖未启用；`IPV6_MULTIPLE_TABLES=n`|能按源地址/mark 分流，支持 VPN 路由和按需的多 WAN 策略|
|VRF、ECMP 等进阶路由|当前配置未形成相应支持|在确有多 WAN/租户隔离需求时启用|
|WireGuard|`WIREGUARD=n`|优先补一个可靠、可测吞吐的现代 VPN 路径|
|OpenVPN 类 TUN 用户态 VPN|`TUN=n`|启用 TUN 并集成用户态服务即可建立基础能力；性能单独测|
|IPsec|`XFRM_USER=n`、IPv4/IPv6 ESP 关闭、`CRYPTO=n`|有站点互联需求再补 XFRM/ESP 和算法；CPU 算法加速需核实实际 CPU 能力|
|IPIP/GRE/SIT/L2TP/VXLAN 等隧道|当前相关配置普遍未启用|按所需协议启用，不要求一次性全开|
|跨接口 IPv4/IPv6 路由多播|`IP_MROUTE=n`、`IPV6_MROUTE=n`|需要 IPTV/跨 VLAN 多播时，先补软件 multicast routing 与控制守护|

证据：[路由与 XFRM 配置](/Volumes/code/zx279133/out/kernel/.config:848)、[VPN/TUN 配置](/Volumes/code/zx279133/out/kernel/.config:1438)、[crypto 配置](/Volumes/code/zx279133/out/kernel/.config:2803)。`IP_ADVANCED_ROUTER` 本身是配置菜单开关，真正所需的是它控制的 policy-routing 等选项；不能把关闭该选项理解成普通 IPv4 路由不能用。

Linux bridge 的 IGMP/MLD snooping 到 RTL MDB 已打通，属于二层组播。跨 WAN/LAN 或跨子网的路由多播是另一项能力。RTL 自主私有组表刻意关闭，由 Linux bridge 统一管理，这是合理的控制面设计。[MDB 生命周期](/Volumes/code/zx279133/port/mainline/network-acceptance/MDB.md:152)

硬件限制不能靠配置解决：NPPT 目前只接受 LAN↔WAN，第二个不同的活动 SNAT 地址、第二个不同的 PPPoE SID 会被拒绝。软件先支持复杂路由，未支持的组合应可靠地走软件；多 WAN 硬件加速需要重新建模 WAN context 和资源归属。[端口模型](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-offload.c:594)、[单 SNAT/SID 限制](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-offload.c:625)、[方向限制](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-offload.c:832)

## 4. CPU 软件转发效率：普通网卡层仍有实质缺口

**缺口性质：驱动实现；硬件可支持到哪里仍需证据。**

只要用了 SQM、VPN、透明代理、特殊协议，或者新连接尚未卸载、硬件表已满，就需要 CPU 路径。NPPT 大流转发快，不能代替这条路径的效率。

|能力|代码中的实际状态|优先级判断|
|---|---|---|
|RX checksum|未广告 `NETIF_F_RXCSUM`，没有完成可信 RX 校验结果到 skb 的契约|优先恢复 descriptor 状态含义，再测正确/错误校验、IPv4/IPv6、分片与封装|
|TX checksum|仅非封装 IPv4/TCP 硬件生成；其余走 `skb_checksum_help()`|完善 UDP/IPv6 是有价值的方向，是否可硬件实现需恢复协议|
|Scatter-gather DMA|TX 先 `skb_linearize()`，没有 SG descriptor 链支持|能减少非线性 skb 复制，优先于追求所有高级 ethtool 接口|
|TSO/TSO6|没有对应硬件分段实现与广告|应在 checksum、SG、completion 契约成熟后评估，不预设芯片支持|
|RSS/多队列|单 Linux RX queue，WAN/LAN 共用一个 IDM TX 硬件队列；无 RSS 接口|先测单队列瓶颈；确有收益再做 RX 分流、多队列/中断建模|
|软件分流|`RPS/RFS_ACCEL/XPS=y` 已编入|可先调 IRQ/RPS 并测效益；配置开启不等于实际已正确配置，更不等于硬件 RSS|
|动态 RX admission|无 `ndo_set_rx_mode`，固定 NPPT admission 会向 CPU 送 foreign unicast/multicast|有助于降低无关包和洪泛带来的 CPU 消耗；不能据此推断现有 VLAN 隔离失效|
|ring/coalescing/channels 调节|ring 可查询，缺设置；缺 coalescing/channels/RSS 调节|按实际丢包、延迟与 IRQ 瓶颈补，不为了接口齐全强行实现|

证据：[checksum 判定、线性化与共享 TX 队列](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-netdev.c:245)、[feature 广告](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-main.c:489)、[ethtool 与 RX admission](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-netdev.c:1273)、[RPS/XPS 配置](/Volumes/code/zx279133/out/kernel/.config:1111)。RSS 与 RPS 的硬件/软件职责区别可对照 [Linux 网络扩展文档](https://docs.kernel.org/networking/scaling.html)。

旧文档把“SG/TSO/GSO/LRO 未实现”合并在一起，表达不够精确。GSO 本来就提供软件分段 fallback，不能写成内核连软件 GSO 都没有；这里缺的是 NIC 硬件分段及相关数据路径支持。GRO 已经存在。LRO 也不应成为普通路由器的必补目标。[本地内核分段说明](/Volumes/code/zx279133/linux-6.18.38/Documentation/networking/segmentation-offloads.rst:116)

## 5. 硬件加速和路由策略的一致性：需要单独闭合

**缺口性质：集成与验收；本次没有证明以下场景存在具体 bug。**

已验证 add/delete/flush、老化和表项复用，值得保留。进一步成为日常路由内核，需要回答下面这些使用中经常发生的问题：

|场景|需要保证的结果|
|---|---|
|连接已经硬件卸载，此时更新防火墙或访客策略|新策略按照明确约定作用于既有连接；需要清流时能完整、定向清理|
|启用/修改 SQM、限速、统计或透明代理|需经过 CPU 的连接退出硬件快路径；其余连接可继续加速|
|DHCP 换 WAN 地址、PPPoE 重拨换 SID、网关 MAC 改变、路由切换|旧 rewrite/context 不继续转发，不串 WAN，新流及时恢复|
|VLAN、bridge、LAG 或出口设备变化|正确失效相关流，隔离关系和回程口不依赖旧缓存|
|MTU/PMTU、TTL/Hop Limit、分片、IPv6 扩展头、FIN/RST 等异常路径|返回/回落正确，不因加速造成黑洞；IPv6 的必要 ICMPv6 能到达端点|
|双向安装中一侧资源不足、表满、删除失败、驱动恢复|不留下半条错误连接或旧动作；软件回落、资源释放和恢复结果可观测|

Linux 官方说明：flowtable 命中会绕过 ingress 之后的常规 netfilter 转发 hooks；硬件卸载还存在目的 MAC 或输出设备变化后缓存过时的边界。因此，“防火墙能加规则”和“硬件转发能跑满”不能独立证明两者组合后的策略正确。[Linux flowtable 文档](https://docs.kernel.org/networking/nf_flowtable.html)

这些并不全由驱动单独负责。内核 flowtable、netdevice 生命周期和用户空间的清流/重建策略都参与其中。建议建立少量真实场景回归，避免仅因驱动内没看到某一种 notifier 就判定已有 bug。

## 6. 容量、统计和故障定位：已有基础，仍有边界

**缺口性质：资源限制 + 可观测性 + 有针对性的验收。**

- SDT43 的 4,096 directions 与 SDT14 DDR 是不同资源路径。已有 4,300 双向 PPPoE 连接、8,600 directions 的板测；262,144 个 DDR buckets 是表布局，不是已经验证的可用连接数，更不能直接除以二当作可靠并发容量。
- 精确 SDT29 packet/byte counter 只有 1,024 个方向 ID。超出后可用 age 维护 `lastused`，但不能据此提供每条连接完整精确流量账单。
- RTL ACL 有 96 TCAM 槽，复杂匹配会消耗多槽；32 个 packet logging counter，不应宣称 96 条任意规则均带独立完整统计。
- RTL logging 和 per-rule policing 共用 action selector；当前 packet 模式没有 byte counter。`tc -s` 字节为 0 不等于未命中。
- 已有标准 stats、MIB、register dump 和部分恢复机制。下一步更实用的是暴露硬件/软件流数量、资源用量、失败原因、回落原因、队列 drops 和恢复计数。devlink health/resource 是可选的标准承载方式，不必先为接口本身投入一整轮开发。

证据：[NPPT 容量与验收](/Volumes/code/zx279133/docs/SR1010_KERNEL_CAPABILITY_AUDIT_2026-08-31.md:401)、[统计资源常量](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133.h:341)、[ACL 统计边界](/Volumes/code/zx279133/port/mainline/network-acceptance/RTL8372N_ACL.md:160)。

验收优先关注连接建立速率、短连接回收、混合硬件/软件流和小包 PPS。现有大包吞吐证据很有价值，但无法直接给出这些指标。先做短时、分组、可复现测试；出现内存或计数漂移再延长稳定性测试。

## 7. 长期运行所需的产品内核与系统集成

**缺口性质：配置、存储路径和用户空间；不应全部计入网卡驱动。**

当前初始化文件明确是诊断启动：静态 WAN 地址、debugfs、直接 shell、`telnetd -l /bin/sh` 和 iperf3 服务。调试用途明确，但产品启动需要认证的管理服务、服务监督、合理的默认网络策略和故障信息留存。[当前 init](/Volumes/code/zx279133/out/rootfs/init:1)

内核方面，seccomp、FORTIFY、HARDENED_USERCOPY 等未启用；保留了 `/dev/mem` 且未启用 STRICT_DEVMEM。应从实际管理服务、模块加载和调试需求出发形成独立产品配置。没有必要把所有 cgroup、namespace、LSM 或容器功能一次性全开。[seccomp 配置](/Volumes/code/zx279133/out/kernel/.config:548)、[内存接口](/Volumes/code/zx279133/out/kernel/.config:1829)、[加固配置](/Volumes/code/zx279133/out/kernel/.config:2788)

持久化方面，当前是 RAM FIT/initramfs，SPI-NAND 路径以只读验证为边界，`MTD_UBI=n`、`OVERLAY_FS=n`。如果最终从 NAND 运行，需要完成 NAND/ECC/坏块、可写配置、更新失败恢复等路径；使用外置 ext4 等方案则应按实际介质选型，不必强制 SquashFS+overlay。USB 读取通过不能替代文件系统写入验证。[存储配置](/Volumes/code/zx279133/out/kernel/.config:1340)、[文件系统配置](/Volumes/code/zx279133/out/kernel/.config:2624)

另外需要可追踪的内核版本、配置、补丁、FIT 与测试记录；定期评估所用稳定分支修复并做短回归。当前大量功能处在未提交工作区，影响可复现版本和回退能力；这是一项交付缺口。本次没有执行提交、升级或任何持久化板端写入。

## 已实现但还缺最后证据的项目

这些应列入补测，不应再排成从零开发任务。

|项目|已有证据|还需补的实际场景|
|---|---|---|
|LAG|三种 hash 映射、成员/active mask、FDB 迁移、link flap、删除复用与 teardown|双在线成员实际按 hash 分流；不能把 balance-xor 的验收扩成所有 bonding/LACP 模式|
|MDB|真实 IGMP/MLD join/leave 自动装卸硬件表，已有表命中 A/B|两个 LAN 端点之间正确复制、非成员收不到、退出后停止复制|
|mirror|规则编程、计数和原始路径保留|镜像目标端点真正收到副本|
|ETS/WFQ|队列/权重读回、单队列硬件限速 A/B|两个同时持续有流量的类别实际带宽比例和 strict-priority 行为|
|C-VLAN pop|硬件编程与 packet-hit|正确 tagged fixture 下的端点交付和删规则恢复|
|EEE、四端口与速率矩阵|有配置生命周期、部分链路切换和收发证据|按需要补实际 LPI 对端、端口等价性、10/100 与混合负载；不将验收表中的“通过条件”自动当作已通过记录|

LAG 源码只接收 hash-based 类型，已有 active-member 切换不等于已经实现 active-backup 硬件卸载。[LAG 类型限制](/Volumes/code/zx279133/linux-6.18.38/drivers/net/ethernet/zte/zx279133-rtl8372n.c:5180)

新 S-VLAN transport 已做 NPPT IPv4 硬件命中和校验回归，不能说完全未回归；但该记录明确不是新测出的最终吞吐上限，也不覆盖最终 FIT 的全部 IPv6/PPPoE/容量组合。[transport 回归](/Volumes/code/zx279133/docs/mainline-network/NPPT_FORWARDING.md:883)

## 当前可后置的功能

|功能|判断|
|---|---|
|RTL8372N 完整 PTP/包硬件时间戳|本板 TOD 不走、外部源 apply 超时。属于已观测到的时钟限制；没有新板级资料前不值得继续堆接口。SoC 基础 PHC 保留|
|WoL、suspend、9K jumbo、EEPROM 接口|不是常规有线路由必需；当前约 1970 用户 MTU 上限足以覆盖普通以太网及常见封装需求，具体叠加封装仍需核算|
|CBS/TAPRIO/ETF、frame preemption 等 TSN|面向明确的实时以太网场景，优先级低于 SQM 和正常路由策略|
|LRO、NAT66、把 ICMP/分片全做硬件卸载|不作为普遍目标；先保证标准软件路径、错误反馈及必要性能|
|原厂 FFE/CSPKernel/私有 proc/ioctl、DPI、全锥 NAT|按具体产品需求决定；标准 nft/tc/DSA 已能承担大量控制职责，不必为“原厂有”而复刻|
|Wi-Fi、PON MAC/OMCI/OAM/DBA、PCIe/RapidIO、NAS 文件服务|属于额外产品范围；单纯评价以太网路由内核时不列入核心欠账|

PTP 实机结论及条件见 [能力边界](/Volumes/code/zx279133/docs/SR1010_KERNEL_CAPABILITY_AUDIT_2026-08-31.md:332)。

## 建议的下一轮顺序

1. **补通用路由配置和一套日常策略**：MASQ、常用 nft 表达式、conntrack netlink、IPv4/IPv6 多路由表、CAKE/FQ-CoDel/IFB；验证动态 WAN、访客隔离和基础 SQM。
2. **把加速与策略联动验清楚**：正在加速的流遇到改规则、开 SQM、换 WAN 地址、重拨、换网关 MAC 和改变出口，能够按预期退出、更新及恢复。
3. **补 CPU 路径**：RX checksum、TX UDP/IPv6 checksum、SG 按硬件证据逐项推进；同时测软件路由/SQM，结合结果再决定 RSS/多队列与 TSO 投入。
4. **加一个完整 VPN，再扩业务**：优先 WireGuard/TUN；IPsec、透明代理、IPTV、多 WAN 硬件 context 按实际使用价值依次推进。
5. **补现有功能的多端点验收并形成可复现版本**：LAG/MDB/mirror/WFQ/C-VLAN pop，随后做混合短连接与必要的恢复测试；产品存储、启动和加固另建配置。

如果下一轮只能选一件事，选择“**让 SQM、日常防火墙、动态 WAN 与硬件加速共同可靠工作**”。如果继续聚焦驱动开发，选择“**CPU 收发路径的 checksum 与 SG**”，随后根据实际瓶颈决定多队列，而不是继续扩 RTL8372N 的低收益外围接口。
