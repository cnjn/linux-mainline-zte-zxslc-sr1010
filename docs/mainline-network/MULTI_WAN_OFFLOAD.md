# 多拨与双 WAN 的硬件卸载（2026-10-15）

两种场景：

- **单口多拨**：WAN 口（eth0）上建多个 macvlan，每个带自己的 MAC，各拨一路 PPPoE（常见于 mwan3 负载均衡）。
- **LAN 口当第二 WAN**：某个 LAN 口（实验用 lan4）移出 br-lan，配成第二个 WAN（IPoE 或 PPPoE）。

## 1. 单口多拨（macvlan）

需要三处改动，缺一不可。

1. **内核 macvlan：`ndo_fill_forward_path`**（`drivers/net/macvlan.c`）。上游 macvlan 没有实现，flowtable 解析路径到 macvlan 就停了，交给驱动的出口是 macvlan 而不是 eth0，驱动不认。补上后报告一跳 `DEV_PATH_ETHERNET`，flowtable 用 macvlan 的 MAC 作源 MAC、eth0 作出口。
2. **firewall4**（`package/network/config/firewall4/patches/002-fw4-resolve-macvlan-lower-device.patch`）。fw4 选硬件卸载设备时只对 `vlan`/`bridge` 找下层设备，netifd 把 macvlan 报成 `devtype ethernet`。补丁按 `type == macvlan` 也往下找，flowtable 设备列表变成 eth0 而不是 mv1/mv2。**必须和第 1 条一起用**：只打 fw4 补丁、内核没补时，路径解析停在 mvX，而 mvX 已不在 flowtable 里，连软件卸载都会失效。
3. **驱动：ONU MAC 槽**。NP 的 WAN 入口程序只对目的 MAC 在 SPA ONU MAC 槽（`0x8300 + 8*i`，共 16 个）里的帧做硬件转发，其余交给 CPU。驱动原来只用 0、1 两个槽（eth0 的 MAC 和 MAC+1）。现在 eth0 实现了 `ndo_set_rx_mode`：macvlan 会把自己的 MAC 挂到 eth0 的单播地址表上，驱动用工作项（需要 `datapath_lock`）把地址表写进 2–15 号槽，硬件重新初始化（`spa_prepare`）时也重写一遍。超过 14 个地址时，多出来的照常收包，只是不加速。

**实测**（kexec 内核，mac 在 lan2，两路 PPPoE 会话 vwan1/vwan2，MAC …:51/…:52）：

| | 没有第 3 条 | 三条都有 |
|---|---|---|
| 上行 | 硬件；但下行 ACK 走 CPU，每路只有 52 Mbit/s | 1105 + 1227 = 2.33 Gbit/s |
| 下行 | CPU，733 + 608 Mbit/s，软中断 50% | 1015 + 937 = 1.95 Gbit/s |
| CPU | 忙 | 空闲 |

抓包确认每个会话的上行帧带自己的 MAC 和会话号（…:51 ↔ 会话 4，…:52 ↔ 会话 2），与 WANID 表一致。上行用 WANID 1/2（各自 MAC、会话号、源地址），下行共用 WANID 17（PPPoE pop）。

这次 kexec 测试跑完负载后，路由器静默复位了一次（串口无 panic），和已知的 kexec 现象一致；需要在冷启动的正式固件上复测排除。

## 2. LAN 口当第二 WAN

### 2.1 驱动改动

- 允许 **LAN 入、LAN 出**的路由流（`lan_routed`）。WAN→WAN 仍然拒绝。
- **LAN 入口的表项一律设 `wan_mode`**（byte 29 bit 1），出口端口由 byte 20 决定：出 WAN 填 `zx279133_tx_port`，出 LAN 填目标口的传输 VID。不设 `wan_mode` 时出口端口被清零，帧丢失，与 §1.6 记录的 LAN→WAN 现象相同，出口是 LAN 口时也一样。实测：不设时 lan2↔lan4 两个方向都命中流表但帧消失；设了以后两个方向 2.35 Gbit/s。
- LAN 入口始终设 sport 改写（与 np.ko 一致）；dport、sip、dip 视实际是否被改写而设，覆盖第二 WAN 的 SNAT/DNAT。
- 主机表项（SDT 9）改为对所有"出口是 LAN 口"的流安装，第二 WAN 后面的网关（实验中是笔记本，59 号端口）也会得到表项。
- LAN 入口表项一律按回包键建索引，并使用上行 WANID 表（0–15）。

### 2.2 PPPoE 第二 WAN 只能走 CPU

- **加头**：往 LAN 口出的 PPPoE 加头表项（multi + IKEY，`wan_mode`）装得进去，但**从不命中**：单向 UDP 75 秒期间软中断约 45%，流每秒被软件路径刷新一次，表项统计始终为 0。硬件只在 WAN 口出口路径上加 PPPoE 头。
- **去头**：LAN 入口程序不去 PPPoE 头，PPPoE 帧按内层五元组匹配不到，交给 CPU（不会被误转发）。
- 因此驱动拒绝出口为 LAN 口的 PPPoE 加头，但留一条不占硬件资源的"已拒绝"记录（`refused`，按回包键索引）。反向规则（PPPoE 入口，规则本身看不出是 PPPoE）查到它就一并拒绝，避免为每条连接装一条永不命中、却占 IKEY/ZCAM 槽的表项。记录在 DESTROY 或流表解绑时释放。

TCP 走 CPU：上行约 600–640 Mbit/s，下行约 1.15 Gbit/s。

**未解释、保留**：加头表项还在时，TCP 测试期间路由器侧 lan4 软件发送计数为 0、lan4 上 tcpdump 看不到包、按 /proc/stat 算 CPU 也几乎空闲，与"表项不命中"矛盾。UDP 测试的结论（软中断 45%、每秒刷新、统计为 0）更直接，以它为准；完全弄清需要 PPU 单步跟踪。

### 2.3 实测（kexec 内核，最终代码）

笔记本插 lan4，第二 WAN：IPoE 192.168.175.5/24、PPPoE `wan2ppp`。mac 在 lan2。

| | 上行 | 下行 |
|---|---|---|
| IPoE 第二 WAN | 2.35 Gbit/s，硬件，CPU 空闲 | 2.35 Gbit/s，硬件 |
| PPPoE 第二 WAN | 638 Mbit/s，CPU | 1.15 Gbit/s，CPU |

测试结束后驱动表项归零，"已拒绝"记录也被释放。

冷启动正式固件上，IPoE 第二 WAN 上下行**同时**跑：上行 1571、下行 536 Mbit/s，合计约 2.1G，而单方向各 2.35G。交换芯片到 NP 的上联是 10GBASE-R，不是瓶颈；两个方向都设了 `wan_mode` 走 WAN 路径，推测共用了按 WAN 口 2.5G 配置的出口调度/整形。待对照原厂做法（把端口配成 WAN 类型）。


## 3. 实验脚本（`port/mainline/pppoe-lab/`）

- `mdial-lab.sh up|down`：在 eth0 上建 mv1/mv2 两个 macvlan（MAC …:51/…:52），各拨一路 PPPoE（vwan1/vwan2）；目标 10.64.100.1 走 vwan1，10.64.101.1 走 vwan2。iperf3 服务端：5201 测上传，5203 测下载（一个服务端同时只服务一个客户端）。
- `mdial-run.sh [秒]`：从 mac 同时对两路做上传和下载，共 4 条流。
- `wan2-lab.sh up|down`：lan4 移出 br-lan，配成 wan2（IPoE）和 wan2ppp（PPPoE）。路由器经 mac → 192.168.5.1:22（LAN 侧）访问，换线时不会失联。
- `udp-blast.py`：笔记本→mac 方向的 UDP 发包工具，用来分方向判断。
- `kexec/kexec-boot.sh`：`ROUTER` 可用环境变量覆盖，笔记本不在 WAN 口时改走 LAN 侧访问。

## 4. 顺带发现

- **交换芯片泛洪（已修）**：mac 从 lan2 发往路由器 MAC 的帧，在接 lan1 的 mac en0 上也能抓到（冷启动正式固件上，10 个 ping 在 lan1 上出现 11 帧）。原因是 `zx279133_rtl8372n_port_fdb_add/del()` 遇到 CPU 端口直接返回 0：DSA 下发的桥主机地址（br-lan 的 MAC）没写进交换芯片，CPU 端口又不学习源 MAC，发给路由器的单播就成了未知单播，泛洪到所有桥接口。修复后 CPU 端口的地址按端口 8 写成静态条目；ping 加 842 MB iperf，lan1 上不再出现。独立端口（如做第二 WAN 的 lan4）有端口隔离，原本就不会泄漏。
- 写过的实验脚本里，`pkill -f <模式>` 两次匹配到了执行它的远程 shell 本身，导致后续命令没有执行；模式要写成带方括号的形式（如 `[u]dp-blast.py`）。

## 5. 原厂的做法：把 LAN 口切成 WAN 类型端口（2026-10-15，原厂固件实测）

原厂网页"WAN2（次要）/WAN1（主要）"，在 lan4（原厂 `eth1`，移出 `br0`）上拨 PPPoE（建在 `nbif1` 上，`ppp0` 拿到 10.64.0.2）。mac 在 lan2，经它到 10.64.101.1：**上行 2.34 Gbit/s、下行 1.89 Gbit/s，CPU 100% 空闲**，确实是硬件转发。

**快表**（`/sys/devices/platform/np/test/showfasttableall`，打到 dmesg；完整导出见 `vendor-reference/vendor-wan2-pppoe-20261015/showfasttableall.txt`）：

| | 上行 lan2 → WAN2 | 下行 WAN2 → lan2 |
|---|---|---|
| wan_mode | 1 | 0 |
| gemport（出口） | 59（lan4） | 61（lan2） |
| len_changed | 4（加头） | 4（去头） |
| 改写 | sip、sport、smac、dmac | dip、dport、smac、dmac |
| WANID | 1：会话 1，m_pppoe 1，sip 10.64.0.2，MAC …:3b | 17：m_pppoe 2，MAC …:3a |

上行表项与主线 §2.2 试过、但从未命中的加头表项结构相同；下行就是普通 WAN→LAN 的去头表项。差别在 59 号端口本身。

**配置差异**：用 vdump2–5 同一套脚本（`vendor-wan2-pppoe-20261015/vdw/`），与当天早些时候 LAN 模式下的导出（`eram-20261015/`、`vendor-pc10-20261015/pro.txt`、`regs-20261015/`、`vendor-spa-port-attr-mems16-63-20261015.txt`）逐项比较。属于配置、并且与端口有关的只有三处，都针对逻辑端口 59：

1. **协议表（SDT 4，`59*128 + 原因`）**：整张换成 6 号（WAN 口）的表，逐项相同（`7c3c 7c3c 7c3c 3030 3030 7030 …`，原因码 30/33 = `7436`）。
2. **MF 扩展表（SDT 50 第 59 项，也就是 SPA 端口属性 mem 59 的 70/71）**：`0x1c8c2800 0x00000a38 0x2f235600 0` → `0x148e2000 0x00000a39 0x2f235600 0`。第 0 字与 6 号 WAN 口的 `0x148e0000` 同类（异或差 `0x08020800`），第 1 字 bit 0 置 1。60–62 号不变。
3. **SDT 6 第 59 项**：`0x82000000` → `0x02000000`（bit 31 清零）。60–62 号仍为 `0x82000000`，6 号为 0。

其余差异都不是转发配置：
- SDT 1 第 3、4 字、nppu `0x810`、所有端口的属性 42（`0x1f4000c → 0x1ff000c`）、WANID 的 mtu（`0x7cc → 0x7fc`）是同一次 MTU 调整（0x1ff × 4 = 2044）。
- SDT 2 是 WANID 表（动态）。
- SPA `0x805c` 是间接访问数据寄存器的残留值。
- tm/red/qmg/sopc 的差异是计数器和队列状态；xmac1 的差异来自 WAN1 断开。
- **SPA 本机 MAC**：0x8300 起的一组在两种模式下都是 …:3a/…:3b；0x8340 起的一组（`sw_mac_add_cpu_macaddress` 的 8 个槽，按 WAN 编号）在 LAN 模式下第 0 槽为 …:3a，在第二 WAN 模式下第 0 槽清空、第 1 槽为 …:3b（原厂 WAN2 用 MAC+1）。主线的 macvlan 地址写在 0x8300 组的 2–7 槽；0x8340 组语义不同，不应混用（待改：最多 6 个 macvlan 地址）。

原厂 `tm_sw_double_wan_set()` 另外还设置了 `lan_up`、`lan_up_port`、`lan_up_other_port`、`g_pon_work_mode = 16`，并调用 `spa_set_down_port_config()` 等，这些在 dump 里没有对应的配置差异，大概是软件状态。

**主线要做的**：在 lan4（一般地说，用作 WAN 的 LAN 口）上写上述三处，把它变成 WAN 类型端口。驱动对这个口的流按 WAN 处理：从它进来的流建成 WAN→LAN 下行表项（PPPoE 用 full 表项去头），出到它的流建成加头的上行表项（wan_mode 1，gemport = 端口号）。

## 6. 主线实现：WAN 类型的 LAN 口（2026-10-15）

**驱动**
- `zx279133_lan_vport_wan_set()`（np.c）：把一个逻辑 LAN 口切成 WAN 类型或切回，写 §5 的三处（协议表用 `zx279133_wan_proto_table`，并给原因码 30/33 置 ppsnum 位；MF 扩展表按 `0x08020800` 翻转第 3 字、第 2 字 bit 0 置 1；SDT 6（基址块 23026）word 3 bit 31 清零或置位），切换后清空所有卸载流（`zx279133_flow_offload_reset()`）。状态存在 `eth->lan_wan_vports`，硬件重新初始化时由 `zx279133_lan_vport_bringup()` 重新应用。
- 接口：`/sys/class/net/eth0/device/wan_ports`。写 `"lan4 1"` 切成 WAN 类型，写 `"lan4 0"` 切回；读出当前为 WAN 类型的口名。
- 流卸载：`zx279133_flow_dev_port()` 把 WAN 类型的 LAN 口算作 WAN，并带出各自的出口号（gemport）：eth0 为 `zx279133_tx_port`，LAN 口为传输 VID。于是第二 WAN 的流就是普通的上行（加头，`wan_mode`）和下行（去头，full 表项）。WAN↔WAN 仍拒绝。
- **接收**：WAN 类型 LAN 口送 CPU 的帧进 **WAN 组队列（8–15）**，word 1 里仍是传输 VID（59），帧完整，没有 LAN 路径那种被吃掉的 4 字节。原先按队列号判断 LAN/WAN，这些帧会被交给 eth0（路由器 ping 不通 lan4 上的网关、PPPoE 会话断）。现在 word 1 是 59–62 时一律按 LAN 帧交给对应 DSA 口；只有 LAN 组队列（0–7）的帧才做 L2 恢复。由 PPU 跟踪定位：同一个 ICMP 帧，LAN 类型最终队列 `0x104`，WAN 类型 `0x10d`。

**ImmortalWrt**（`target/linux/zte/zx279133/base-files`）
- `/usr/libexec/zx279133-wan-ports`：找出防火墙 wan 区域（名为 wan 或开了 masq）里各接口的设备，沿 VLAN/macvlan 往下找到交换口，写 `wan_ports`；其余交换口写 0。
- `/etc/init.d/zx279133-wan-ports`（`procd_add_reload_trigger firewall network`）和 `/etc/hotplug.d/iface/50-zx279133-wan-ports`（ifup 时再跑一次），`uci-defaults` 启用。
- 触发要靠 `reload_config`（LuCI 保存并应用即是）；直接 `/etc/init.d/firewall reload` 不发配置变更事件，不会触发。实测：把 wan2/wan2ppp 移出 wan 区域并 `reload_config`，lan4 切回 LAN 类型；移回后切成 WAN 类型。

**冷启动正式固件实测**（cf598c9c…，笔记本插 lan4，mac 在 lan2；单位 Mbit/s；CPU 按 /proc/stat 2 秒差值）：

| | PPPoE 第二 WAN | IPoE 第二 WAN |
|---|---|---|
| TCP 上传 | 2336 | 2353 |
| TCP 下载 | 2339 | 2353 |
| UDP 上传（-l 1400） | 2374，丢 0% | 2387，丢 0% |
| UDP 下载 | 2090，丢 0.001% | 2007，丢 0.016% |
| TCP 同时上下行 | 1773 + 530 | 1597 + 530 |
| CPU | 0–2% | 0–2% |

上行帧：`0x8864`、会话 1、PPP `0x0021`、SNAT 源地址正确。

**同时上下行**：TCP 合计只有约 2.2G（原厂同测 2289 + 1631）。但双向 UDP 能做到上 2300（丢 0%）+ 下 1875（丢 1.6%，单向 UDP 下载本来就约 2.0G，是笔记本发送端的上限）；交换芯片 CPU 口与 lan2/lan4 在测试期间没有任何丢弃、错误或流控计数，只有 `p08_rx_oversize_frames`（带传输标签后超过 1518，属正常）。所以硬件双向容量够，TCP 的差距来自下载方向的 ACK 与上传数据排在同一路径上、时延变大，原厂的队列/优先级配置可能不同，待查。

**已知限制**
- WAN 类型 LAN 口的 MAC 若改掉（克隆 MAC），或在它上面再建 macvlan 多拨，这些地址不会写进本机 MAC 槽，下行走 CPU。
- 被切成 WAN 类型的口若其实是一个独立内网段，它到 eth0 的流量是 WAN→WAN，走 CPU；所以只按防火墙 wan 区域切换。
- 60–62 号口的 MF 扩展表 WAN 值是按 59 号的翻转规律推断的，没在原厂上实测。
- macvlan 地址现在只用 0x8300 组的 2–7 槽，最多 6 个。

## 7. 收尾（2026-10-15，最终固件 470594e2…）

**网桥保护**：WAN 类型只在端口不在网桥里时才有意义。驱动在 `port_bridge_join` 时自动把端口切回 LAN 类型；`wan_ports` 拒绝把已桥接的口切成 WAN 类型（`-EBUSY`）；脚本遇到网桥不往下解析（否则 wan 区域里若有网桥，会把它的成员口误切）。起因：实验脚本把 lan4 放回 br-lan 时只做了 `network reload`，既没有 `reload_config` 也没有 lan 的 ifup，lan4 留在了 WAN 类型。kexec 实测：桥接时写入返回 EBUSY；离开网桥可以切；重新加入网桥自动切回。

**单口多拨（macvlan）冷启动复测**（笔记本回到 WAN 口；单位 Mbit/s；CPU 1–2%）：

| | vwan1 | vwan2 | 合计 |
|---|---|---|---|
| 单路下载 | 2340 | 2340 | — |
| 两路同时下载 | 1170 | 1170 | 2340 |
| 两路同时上传 | 1204 | 1137 | 2341 |
| 两路 UDP 下载各 1200M | 1200 | 1174 | — |
| 2 上 + 2 下 | 上 1166 / 下 1138 | 上 1165 / 下 1097 | 上 2331 / 下 2235 |

下行已是线速。§1 里"下行合计 1.95G"是在 kexec 内核上测的，作废。冷启动上连续负载 1 小时无复位、无内核告警，kexec 上那次静默复位与新代码无关。

**eth0 回归**：IPoE 上 2352 / 下 2353，UDP 上 2386（丢 0%）/ 下 1969；TCP 同时上下行 2347 + 1834（合计 4.2G）。PPPoE 上下行 2.34G。泛洪检查：lan1 上只有表项装上前的 1 帧。

**第二 WAN 的 TCP 同时上下行（约 2.2G）与 eth0（4.2G）的差别**：第二 WAN 下载方向的 ACK 要从 lan4 出去，lan4 的 2.5G 出口正被上传数据占满，ACK 在交换芯片 lan4 的出口队列里排队；eth0 出口的排队特性不同。双向 UDP 能跑满，交换芯片两侧无丢包、无流控，所以不是硬件容量问题。原厂同测 2289 + 1631，可能在交换芯片上有不同的队列/优先级设置，待查。

**实验脚本**：`mdial-lab.sh` / `wan2-lab.sh` 的 down 里 `pkill -f` 模式改为带方括号，避免杀掉执行它的远程 shell。

## 8. 第二 WAN 上下行同时跑 TCP 的问题（已解决，2026-10-16，固件 6f5f3065…）

**现象**：第二 WAN（lan3/lan4 为 WAN 类型）上下行同时跑 TCP，上行 1.1–1.8G、下行约 0.5G；原厂同测上 2319、下 1757。

**已确认**
- 不是缓冲膨胀：负载下 RTT 只有 0.7 ms（原厂 3 ms）。
- 不是容量上限：双向 UDP 合计可达 4.17G。
- 主线在 NP 的 **RED 入口丢包**：`0x20208`（in drop）、`0x20214`（out drop）、`0x38110`（DMA wrdrop）同步增长，与下载方向的 TCP 重传对得上。只有下载帧被丢，上传帧不丢；单方向下载时 RED 一个不丢。双向 UDP 下行丢包随总负载增加：各 1200M 时 +28，各 1500M 时 +106，各 1800M 时 +6809。
- **原厂同样负载下 RED 计数完全不增长**，它在排队（RTT 3 ms），主线直接丢。
- `0x2009c`、`0x38164` 只是丢包计数的镜像，不是队列号。
- PPU 输出等价：原厂与主线的下载帧和上传帧在 PPPoE 第二 WAN、双向负载下的单步跟踪，最终描述字 pk0–7 一致，仅有 PPPoE 会话号带来的差别（pk5 = sid<<17|0x8000 等）。原厂跟踪见 `vendor-reference/vendor-bidir-20261015/vtr/`。注意：在原厂上连续跟踪会让其转发卡死，需要重启。

**试过、无效的**（都在运行时写入原厂值后重测）
- RED 全局寄存器 `0x20060/64/70/74` 的高 16 位、`0x20278`、`0x20348`、`0x2034c`；放大 down 入口共享上限、down 出口上限。
- RED 间接表 RAM 0（出口上限，原厂 136–319 号为 0）和 RAM 2（入口门限）全部改成原厂值。
- ssch `0x2c000/14/24`、usch `0x2801c`、uopc `0x30024`、sopc `0x342a4/a8/c8`、sdet `0x2138`、smct `0xc064`、nppt `0x10`；nppt `0xa8–0xb8` 改成原厂值后双向都掉到约 340M（已恢复）。
- 关闭两端 TCP ECN（数据帧 TOS 0x02 → 0）：无变化。pk2 低字节的 0x80 随帧长（大帧）而不是 ECN。

**根因（两个，叠在一起）**

1. **NP 工作时钟只有 125 MHz**。TOPCRM `0x10e1000c` bits 26:24 是 `pon_nppt_wclk_mux`：原厂 `0x06711277`（6 = clk416m，416.67 MHz），主线 `0x07711277`（7 = clk_pon_125m，沿用引导程序留下的值，时钟驱动又把这个复用器注册成只读）。NP 慢了 3.3 倍，双向负载下 RED 入口来不及处理就丢（上面的 RED 丢包全是这个）。运行时改成 416 MHz 后 RED 零丢包，IPoE 双向立刻到上 2345 / 下 1700。400 / 500 MHz 也一样有效，但采用原厂值。PTP 计时不受影响（另有时钟）。
   - 修复：`drivers/clk/clk-zx279133.c` 去掉该复用器的 `CLK_MUX_READ_ONLY`；`zx279133.dtsi` 的 nppt 节点加 `assigned-clocks = <&topcrm ZX279133_TOPCRM_CLK_PON_NPPT_WCLK_MUX>; assigned-clock-rates = <416666666>;`。`pon_woe1_wclk` 挂在同一复用器下，一起变成 416 MHz（与原厂相同）。
   - 注意：kexec 用的是运行中的设备树，这项只能用整包固件验证。
2. **交换芯片 LAN 口没协商 pause**。时钟修好后，PPPoE 第二 WAN 双向仍是上 2337 / 下 160。此时路由器内部一个下载包都没丢：笔记本发出的帧数 = lan4 收 = lan2 发。丢在 mac 的 USB 2.5G 网卡（en8）接收端，mac 抓包能看到缺口，重传的段 TOS 为 0。
   - 触发条件是大量小帧：笔记本对 PPPoE 做不了 GRO（6.8 内核），每 2 个上传段回 1 个 ACK，mac 每秒要收约 7.6 万个 ACK，同时还在 2.3G 发送。IPoE 下笔记本有 GRO，ACK 只有约 5500/s，所以 IPoE 不明显；把笔记本 GRO 关掉，IPoE 也复现（下 390、重传 4553）。
   - 原厂：交换芯片 `0x63fc`/`0x6400`（TX/RX pause 状态）= `0xD0`，即端口 4/6/7 都协商了 pause。lan2（端口 6）`dot3InPauseFrames` 两轮测试涨了 48 万，`dot3OutPauseFrames` = 0。原厂照办 mac 的 PAUSE，所以不丢包。读法：`echo 0x6400 > /proc/rtl8372_debug/asic_reg_read` 后看 dmesg；`echo 6 > /proc/rtl8372_debug/port_stat` 打印端口 MIB。
   - 主线 `zx279133_rtl8372n_phylink_get_caps` 原先故意不给 pause，理由是交换芯片会因 CPU 口拥塞反过来暂停客户端（多流上传掉到 0.95G）。那是在 NP 只有 125 MHz 时看到的。现在 LAN 口加回 `MAC_SYM_PAUSE | MAC_ASYM_PAUSE`，多流上传 `-P 8` 仍有 2.33–2.35G，交换芯片 `tx_pause_frames` 不涨。

**修复后实测（固件 6f5f3065…，冷启动，mac 在 lan2，笔记本在 lan4）**

| 测试 | 上行 | 下行 | 备注 |
|---|---|---|---|
| PPPoE 双向 TCP | 2311–2316 | 1816–1831 | 重传约 200；原厂 2319 / 1757 |
| IPoE 双向 TCP | 2329–2331 | 1877–1878 | |
| PPPoE 单向 | 2.34G | 1.93G | 原厂 2.34 / 1.89（§5） |
| IPoE 单向 | 2.35G | 1.93G | |
| 上传 `-P 8` | 2.33–2.35G | — | 交换芯片不发 PAUSE |

- 单向下载从 2.35 降到 1.93：mac 每 8 秒发约 12.8 万个 PAUSE，交换芯片照办了。原厂也是 1.89。以前的 2.35 靠的是无视 PAUSE，代价是双向时下载崩掉。
- 主 WAN（eth0）和单口多拨的复测见第 10 节（2026-10-18，冷启动整包固件，通过）。

**调查用的 NP 调试手段**（实验内核，未提交）
- RED 入口描述符快照 `0x20114–0x2011c`：bit0 颜色，bits 1–9 队列，bits 10–23 帧长，bit24 丢弃，bits 25–30 目的端口。
- SIPC 流控计数 `0x41c0–0x41f8`：读清。
- TOPCRM debugfs `zx279133-topcrm`。

**实验环境的坑**：mac 有 en0（lan1，千兆）和 en8（lan2，2.5G）两个口在同一网段，macOS 可能把 10.64/16 的路由放到 en0，所有测试被限在 1G。`pppoe-lab.sh` 已改为把路由钉在 en8（`-ifp en8`）。
- mac 的 USB 2.5G 网卡（en8）接收能力有限，会大量发 PAUSE；测下载吞吐时它才是上限（约 1.9G）。笔记本 6.8 内核对 PPPoE 没有 GRO，PPPoE 上传会给 mac 带来大量小 ACK。笔记本的 ethtool 在 `/tmp/zx-pkgs/root/sbin/ethtool`（未安装，dpkg -x 解出来的）。

## 9. 流表容量：IKEY 用完后退到 SDT14 full 表项（2026-10-16）

**问题**：普通流（IPoE，IPv4/IPv6，两个方向）和 PPPoE 上行（加头）都用 SDT43 multi 表项，每个方向占 4096 项 IKEY 表中的一项。IPoE 一条连接占两项，所以只能硬件加速约 2048 条连接，再多的 `flow add` 返回 `-ENOSPC`，走软件。

**原厂**（`fast_table_write()`，np.ko `.text 0xa4c44`）：
- 先分配 IKEY，写 SDT43 multi 表项（`se_hash_multi_table_set(43, …)`）。
- IKEY 用完，或 multi 写不进（释放 IKEY，`ikey = uFAST_IKEY_DEPTH`）时，改写 SDT14 full 表项（`se_hash_table_set(14, …)`）。full 表项前 256 个在 ZCAM，之后进 DDR（262144 个桶）。
- 不分流量类型。原厂开机打印 `ffe max flow cnt is 40000`。

**主线**：`zx279133_flow_storage_alloc()` 按同样顺序分配，PPPoE 加头也一样。PPPoE 去头照旧一律用 full，不占 IKEY。

**更正**：此前认为 PPPoE 加头只能用 multi，理由是“LAN 入口程序只在 multi 路径上加头”。这个结论来自旧的 PC 561 入口：那时 full 表项用 action 1 交给 CPU，用 action 0 则不加头。换成 PC 10 入口后，加头直接用了 multi + action 0，full 从没在 PC 10 下试过。2026-10-16 实测，PC 10 下 full 表项加头正常：
- 加头表项在 ZCAM 和在 DDR 里都正常；
- 硬件计数在涨，CPU 空闲；
- 上行 2.33–2.34G，笔记本收到的是 PPPoE 会话帧。

**实测**（kexec 实验内核，WAN2 IPoE，mac → 笔记本 10.64.200.1:7000 UDP 回显，每个连接一个 socket，每 4 秒一个来回）：

| 测试 | 结果 |
|---|---|
| IKEY 上限强制为 0（实验开关），所有流都用 full | 上 2.35G / 下 1.88G，CPU 约 3%；LAN 入口和 WAN 类型 LAN 口入口的 full 表项都命中 |
| 同上，400 条连接后再跑 iperf | 400/400 `HW_OFFLOAD`；iperf 上行表项在 DDR（bucket 242494），硬件计数 105 万包；上 2.35G / 下 1.93G |
| 3000 条连接（IKEY 4096） | 3000/3000 `HW_OFFLOAD`，4096 multi + 1906 full（1865 在 DDR）；同时 iperf 上 2.35G / 下 1.93G，CPU 约 5% |
| 10000 条连接 | 9994/10000 `HW_OFFLOAD`，4096 multi + 15890 full（15410 在 DDR）。6 条落空：DDR 桶冲突，退回 ZCAM 时 ZCAM 也满了，这几条走软件，回显全部正常 |
| 10000 条连接时 PPPoE 第二 WAN（加头不回退的旧版本） | IKEY 已满，加头被拒绝，PPPoE 连接为 `[OFFLOAD]`（软件 flowtable），上 560M / 下 982M |
| IKEY 上限为 0，PPPoE 加头强制用 full | 400 条 PPPoE 连接全部 `HW_OFFLOAD`；iperf 加头表项在 DDR（桶 255362），硬件计数 107 万包；上 2.34G / 下 1.93G，CPU 约 3% |
| 6000 条 PPPoE 连接（最终版本，IKEY 4096） | 6000/6000 `HW_OFFLOAD`：上行 4094 multi + 1906 full，下行 6000 full，DDR 中 7603 项；同时 iperf 上 2.33G / 下 1.93G，CPU 约 2% |
| 过期回收 | 两轮之后表项都回到 0（DDR 0、IKEY 0），dmesg 无告警 |

**容量**（IPoE 和 PPPoE 相同）
- 4096 个 IKEY 用于 multi 表项；之后是 SDT14 full：ZCAM 里 256 个，其余进 DDR（262144 个桶）。老化位共 262144 个，即最多 131072 条连接（每条两个方向）。OpenWrt 默认 `nf_conntrack_max` 为 65536。
- 实际瓶颈是 DDR 桶冲突。每个方向只能用一个 CRC32 桶，冲突时退回 ZCAM，而 ZCAM 里剩下的空位不到 800 个。
  - 1 万条连接：约 2 万个方向，99.9% 进硬件（实测 9994/10000）。
  - 3 万条连接：DDR 中约 6 万个方向，按生日问题估算约 6000 个冲突，约 10% 的方向走软件。
  - 原厂查找结构相同，容量应当一样；它打印的 `ffe max flow cnt is 40000` 是软件上限。
- 流量计数器只有 1024 对，之后装入的表项只有老化位（`lastused` 照常刷新），转发不受影响。

**剩余限制**
- DDR 每个 CRC32 结果只有一个桶，冲突时退回 ZCAM（4 块 × 5 个候选）；ZCAM 被 multi 表项占满后，冲突的流走软件。
- 实验开关 `zx279133-ikey-limit`（debugfs）只在实验内核里有，没有进主线。

本节的实测都在 kexec 实验内核上；冷启动整包固件上的复测见第 10 节。

## 10. 主 WAN 与单口多拨复测（2026-10-18，固件 f66c96d8…）

冷启动整包固件 f66c96d8…（槽位 1，序号 107），含第 8 节的 NP 时钟和交换芯片 pause 修复、第 9 节的 full 表项回退；网线插回 WAN 口（eth0）：

| 测试 | 结果 |
|---|---|
| eth0 IPoE / PPPoE 单向 TCP | 上 2.35G / 下 1.93G |
| eth0 双向 TCP | 上 2335 / 下 1880 |
| 上传 `-P 8` | 2.35G，交换芯片零 PAUSE、零丢弃：第 8 节当初不给 pause 的原因（125 MHz 时多流上传掉到 0.95G）没有再出现 |
| 单口多拨（macvlan 两路拨号），4 条流 | 上 2333 / 下 1826，CPU 3.8% |
| 10000 条 IPoE 连接 | 9993 条 `HW_OFFLOAD` |
| 6000 条 PPPoE 连接（经 vwan2） | 全部 `HW_OFFLOAD` |

**未定**：单向下载从 2.35 降到 1.93（原厂 1.89），因为交换芯片现在照办 mac USB 2.5G 网卡（en8）发的 PAUSE。只回退 29dcde525（不给 pause）能回到 2.35，但 PPPoE 双向时下载会掉到约 0.16G。目前保留 pause，取舍还没定。
