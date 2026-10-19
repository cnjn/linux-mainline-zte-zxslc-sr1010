# PPPoE 卸载的两个遗留问题：调查记录

2026-10-14。问题来源见 `WANID_PROFILES.md` §6"原来就有的问题"。测试环境：测试机 WAN 接 Ubuntu 笔记本，笔记本跑 `pppoe-server` 和 iperf3；LAN 侧发起端是 mac 的 `en8`（2.5G）。环境由 `port/mainline/pppoe-lab/pppoe-lab.sh up|down` 搭建和拆除；测试内核用 kexec 切换（`KEXEC_TEST_LOOP.md`）。

结论先行：

- **问题 2（pop 计数不准）的根因已查明**：它是问题 1 的直接后果，并且发生在 nf_flowtable 核心，不在驱动里。
- **问题 1（PPPoE 上行不进硬件）尚未解决**：查明了原因（push 用的 action 1 就是"交给本机 CPU"），但让硬件自己加 PPPoE 头的条件还没找到。

## 1. 问题 1：push 的 action 1 = 交给 CPU

主线 push 流的响应里写的是 `action = 1`（byte 31 bit 1:0）。这是在提交 5336656ce（`scale fast-flow offload with DDR`，2026-08-31）里加的，注释只有一句"Local-fast push is action 1"，没有证据。在那之前，2913575d3（08-28）的 push 用的是 action 0。

证据：

- 厂商 `fast_table_entry_print()` 在 action 为 1 时，把 byte 28 的低 4 位当作 **queue** 打印。
- `switch.ko` 的 `sw_acl_l3_hardfast_session_add()` 里，FFE 流带"local"标志时，会话的出口端口写 `0xff`，并指定 CPU 队列（`g_idm_tcp_drop_cpuqueue` 或 8），同时打印 `Up local Fast` / `Down local Fast`。
- 厂商 `fast_hashinfo_set()` 只在 action 为 1、且端口为 254 时，给 byte 27 写一个特殊值。
- 实测：push 流的上行数据全部经过 CPU。测试机软中断约 800 tick / 8 秒；conntrack 里平均每包 17.7 KB，是 GRO 合并后的 skb；吞吐约 800 Mbit/s。

所以主线的 PPPoE"硬件卸载"上行，其实一直是由 CPU 上的软件 flowtable 加 PPPoE 头，`[HW_OFFLOAD]` 标记是误导性的。

### 1.1 快表响应字段（`fast_table_entry_print()` 的字段名与取位）

| 字段 | 位置 | 说明 |
|---|---|---|
| `m_hit` | byte 31 bit 7 | |
| （未命名，5 位） | byte 31 bit 6:2 | 厂商写会话第 0 字节：去 WAN 为 0，反向为 1 |
| `action` | byte 31 bit 1:0 | 0 路由；1 交给 CPU（queue 在 byte 28 bit 3:0）；2 实测丢弃；3 实测同 0 |
| `vlan_action` | byte 30 bit 7:6 | 2 = 剥 tag（主线对 LAN 入口用） |
| `mode` | byte 30 bit 5 | 1 = 用 WANID 的改写（清零后 SNAT 都失效）；0 时 byte 23 是按端口查出的编号 |
| `dmac_en` `smac_en` `dport_en` `sport_en` `dip_en` | byte 30 bit 4..0 | |
| `sip_en` `dscp_tc_en` `ratelmt_en` `flow_stat_en` `vxlan_en` | byte 29 bit 7..3 | |
| `l2tp` | byte 29 bit 2 | |
| `wan_mode` | byte 29 bit 1 | 厂商对上行流按会话字段设置；主线最早设了，2913575d3 删掉 |
| （未命名，3 位） | byte 27 bit 7:5 | |
| `wanid` | byte 23 bit 4:0 | |
| `tcont_llid` / `cfi` / `vid` | byte 24–25 | |
| `gemport` | byte 20–21 | 出口 |
| `len_changed` | byte 16–19 bit 25:0 | 厂商写 `|长度变化| / 2` |
| `dport` | byte 14–15 | |
| `dmac` | byte 8–13 | |
| `dip` | byte 4–7 | |
| `flowid` | byte 2–3 bit 9:0 | SDT29 计数器 ID |
| `sport` | byte 0–1 | |

长度字段：厂商把累计的长度变化（VLAN ±4、PPPoE ±8、DS-Lite ±40）取绝对值后右移 1 位。PPPoE push 同时剥 RTL8372N tag 时净变化 +4，写 2；这和主线一致。

### 1.2 实验：哪些都不能让硬件加 PPPoE 头

测试内核加了只用于实验的 debugfs 开关（`zx279133-push-set28/clr28/set16/clr16`，对 push 响应的 byte 28–31、16–19 做置位/清零；`zx279133-push-wanid-or3`，对 push 用的 WANID 的 word 3 置位）。每种组合开一条新的 iperf3 上传，在笔记本 `enp2s0` 上抓测试机发出的帧：

| 组合 | 笔记本收到的帧 | 结论 |
|---|---|---|
| action 0（无其他改动） | `0x0800`，SNAT 正确，2353 Mbit/s，测试机 CPU 不参与 | 硬件转发，**不加 PPPoE 头** |
| action 0 + `wan_mode` | `0x0800`，线速 | 同上 |
| action 3 | `0x0800`，做了 SNAT | 同上 |
| action 2 | 卸载后无帧 | 丢弃 |
| `mode` = 0 | `0x0800`，**源地址未 NAT** | `mode` 控制是否使用 WANID 改写 |
| `vlan_action` = 0 | `0x8100`（vlan 62）+ IPv4 | tag 原样保留，仍不加 PPPoE |
| `len_changed` = 4 | `0x0800` | 同上 |
| byte 31 bit 6:2 = 1/2/3 | `0x0800` | 同上 |
| WANID `m_rsv` = 16/17/6（厂商对 WAN 口端口 6 查出的值，已回读确认写入） | `0x0800` | 同上 |

注意：action 0 时笔记本照样"收到了"数据，因为目标地址就在它本机，Linux 会接收发给本机地址的普通 IPv4 帧。**只看 iperf 吞吐会误以为 push 成功**，必须看以太网类型。

同时成立的事实：

- WANID 的 SIP、源 MAC 对上行流生效（T3 两个 SNAT 地址按 byte 23 选中的 WANID 正确改写）。
- WANID 的 `m_pppoe=2` 对下行生效：T5 修复前，下行用了 `m_pppoe=0` 的 WANID，PPPoE 帧被原样转给 LAN，连接断开。
- 主线已按厂商实机状态配置 WAN 口（PPU 端口 6）的起始 PC、PRO 表和 `iswanport` 属性（`zx279133_wan_port_bringup()`）。

所以硬件确实读了 WANID，但上行的 `m_pppoe=1` 不起作用。条件很可能在 PPU 微码里，例如只对某类入口端口或某个起始 PC 执行封装。厂商 `np.ko` 里有完整的 push 写法（修改标志 `0x20000` → `m_pppoe=1`、长度 +8），说明硬件设计上支持。

### 1.3 下一步可选方向

- **在厂商固件上做对照**（最直接）：同样的拓扑下，让厂商固件跑 PPPoE WAN，测上行吞吐和 CPU 占用，看它是不是硬件转发；再用 `np.ko` 的 sysfs 把快表项和 WANID 转储出来，与主线逐字段对比。需要往一个槽位刷厂商固件，并在厂商网页上配置 PPPoE。
- 分析 PPU 微码（`mcode_intel.bin`）里 WANID `m_pppoe` 的读取点。指令集是私有的，代价高。

### 1.4 厂商固件对照（2026-10-15）

在同一拓扑下把测试机切到厂商固件（slot 0，V1.0.0.2B5.8000），WAN 配 PPPoE 连笔记本：

- **厂商的上行 push 是硬件做的**：2335 Mbit/s，笔记本收到的是 `0x8864` PPPoE 帧，测试机 CPU 空闲。
- 厂商两个方向都用 **SDT43 多路哈希表项（256 位）+ SDT44 IKEY**，没有任何 SDT14 整行表项（`se_debug/multi_hash_show 14` 报不存在）。转储：`vendor-reference/vendor-hash-tables-sdt43-20261015.txt`、`vendor-ikey-sdt44-20261015.txt`、`vendor-pppoe-fast-table-20261015.txt`。
- 用主线 `zx279133_fast_multi_table_build()` 的打包方式反解厂商表项，完全对得上（IKEY 编号、flowid、端口、MAC 都吻合）。厂商 push 项：gemport 6、WANID 0、IKEY 7、byte 29 = `0x92`（`sip_en`、`flow_stat_en`、**`wan_mode`**）、byte 30 = `0x3a`（**`vlan_action` = 0**）、action 0。pop 项：gemport 62、WANID 16、byte 29 = `0x10`、byte 30 = `0x3d`。
- 注意：多路表项只存响应的 byte 19 bit 7:2 以后，**byte 16–19 的 `len_changed` 根本不进硬件**。厂商打印出的 `len_changed 4` 不来自表项。

在主线上照厂商的写法复现（实验开关 `zx279133-push-multi`：push 改用多路表项 + IKEY），每次都在笔记本上看以太网类型：

| 组合 | 笔记本收到的帧 | 结论 |
|---|---|---|
| 多路 + IKEY，action 1（主线默认） | `0x8864`，约 650 Mbit/s | CPU 加头，同整行表项 |
| 多路 + IKEY，action 0，`wan_mode` = 1（除 `vlan_action` 外逐位等于厂商） | `0x0800`，2351 Mbit/s | 硬件转发，**不加 PPPoE 头** |
| 同上，`wan_mode` = 0 | `0x0800`，2347 Mbit/s | 同上 |
| 同上，`vlan_action` = 0（与厂商完全相同） | 不通 | RTL8372N 的 4 字节 tag 没剥 |
| 整行表项，action 0，`wan_mode` = 1 | `0x0800`，2347 Mbit/s | 表项格式无关 |
| 先 `conntrack -F` 让 push 拿到 WANID 0（`pppoe 1`、sid 3，与会话一致） | `0x0800` | WANID 编号无关 |

端口属性（SPA）对比，`vendor-spa-port-attr-20261015.txt` 与主线 `zx279133-spa` debugfs：

- WAN（XMAC1，mem 6）、CPU（mem 15）**完全相同**。
- LAN（XMAC0，mem 5）不同：厂商 **PC 10、FLOWNUM 1**；主线 PC 561、FLOWNUM 0。**PC 561 就是厂商 WAN 口的解析程序**，也就是说主线把 LAN 帧当 WAN 帧解析。TPID 选择（addr 13/14 = `0x650650`）两边相同。
- PRO 表（SDT4，每端口 128 项）：厂商端口 1–5、7–15 用"LAN 表"，端口 6 用 WAN 表，端口 0 用 WAN 表的变体；主线只给 0、5、10 写了 LAN 表，其他端口全 0。

运行时改 LAN 口属性的结果（每次都恢复为 `0x463` 并确认 IPoE 回到 2349 Mbit/s）：

| mem 5 addr 0 | 其他 | IPoE | push（action 0） |
|---|---|---|---|
| `0x8015`（PC 10，FLOWNUM 1，同厂商） | 原 PRO 表 | 约 180 Mbit/s，查表不命中 | — |
| `0x8015` | 16 个端口全换成厂商 PRO 表 | 160 Mbit/s，查表不命中 | `0x8864`，约 111 Mbit/s（走 CPU） |
| `0x0015`（PC 10，FLOWNUM 0） | | 727 Mbit/s，不命中 | `0x8864`，约 750 Mbit/s（走 CPU） |
| `0x8463`（PC 561，FLOWNUM 1） | | 2349 Mbit/s | `0x0800`，与原来相同 |

所以 FLOWNUM 无关；PC 10 下主线的表项一条都不命中。

**更正（2026-10-15 后续）**：上面曾推测"厂商的 LAN 帧不带 tag"，这是错的。厂商 RTL8372N 寄存器实测：S-VLAN TPID 0x8100、服务端口 8、每端口 SVID 59–62，Realtek CPU tag 未启用（`0x6720` 厂商 0x500 / 主线 0x100，使能位都为 0），与主线相同；`zte_priv_init()` 反汇编也只配置 S-VLAN。所以两边进 XMAC0 的 LAN 帧一样，都带 802.1Q VID 59–62。差别在 NP 内部，见 §1.5。

另：先用 `fw_flashing -f` 在厂商侧重刷才回到 ImmortalWrt，见 `port/sr1010-slot/README.md` 的事故记录。

### 1.5 入口程序决定是否封装 PPPoE（2026-10-15，厂商机上的因果实验）

**已证实**：在厂商固件上，先让一条上传流进入硬件（`0x8864`，CPU 空闲），再把 LAN 入口 SPA mem 5 的起始 PC 从 10 改成 561（主线用的值），这条已卸载的流立刻变成 `802.1Q (0x8100)` + IPv4 从 WAN 发出——不剥 S-tag、不加 PPPoE 头，和主线一模一样；改回 PC 10，恢复 `0x8864`。所以 PPPoE push（以及剥 S-tag）是 PC 10 这条 LAN 入口程序做的，表项、WANID、出口端口属性都相同。主线要硬件 push，必须让 LAN 入口跑 PC 10。

厂商侧其他事实：

- mem 5 就是厂商的 LAN 入口：把它改成 PC 561，新连接全部不通（FLOWNUM 0/1 都一样；把 port 0 的 PRO 表换成主线的 LAN 表也一样）。
- 厂商 PRO 表 port 0 是另一套（PPS set 12），把它换成主线的 LAN 表（`sram_table_set 4 …`）后，PC 10 下 push 照样是硬件（CPU 软中断 +12）。
- 厂商不做硬件 MAC 学习（`HASH_LEARN_SUCCESS/FAIL` 都是 0）；PC 10 每个包都做一次单哈希查找（`SINGLE_HASH_REQ` 与 `MULTI_HASH_REQ` 同量级），查的是 SDT 9 MAC 表。厂商 SDT 9 里只有一条，是 LAN 主机（mac 的 en8）的 MAC，由厂商软件写入（`tm_mactable_add_set` → `mac_table_add`）。
- 微码文件两边相同（md5 `fdfcf5bb…`），厂商只在 `np_ppu_init` 下载一次，运行时不打补丁。

主线上已排除（都配合 PC 10 试过，查表仍全部不命中，IPoE 约 480 Mbit/s 走 CPU）：SDT 50（端口用户区）、16 个端口的厂商 PRO 表、SDT 0 全部 4096 项、SDT 6[48]、SDT 16[4]；SPA/SDET/SIPC/SMCT/ISU/NPPT 中所有与厂商不同的寄存器（共 56 个，含 0x8340 路由器 MAC）；XMAC 0x10–0x1c bit 9；mem 1–4 的 TPID 选择与长度过滤。

主线 PC 10 下的 SE 计数（PPS 0x40180 起，`fpga_pps_read_reg(0x10060+n)` 即 PPS + 4×ID）：出现单哈希查找（5 秒约 1 万次），而 PC 561 下几乎为 0。往主线 ZCAM（block 0 / cell 0 / 0x7f，与厂商位置相同）写入厂商那条 LAN 主机 MAC 表项后，PC 10 下 LAN 完全不通——说明 MAC 表确实影响 PC 10 的路径。

SDT 9 表项格式（由 `mac_table_add`/`mac_table_get` 反汇编得到）：key 8 字节 = MAC（低字节在前）+ 12 位 VID；存储的响应是写入格式右移 18 位（去掉老化前缀）后的 7 字节：bit 14–23 一个 10 位字段、bit 24–35 VID、bit 36–41 端口（`getPort()` 后的 PPU 端口）、bit 8–15 若干标志。厂商那条：端口 0、10 位字段 3、**响应 VID 0x3e0（992）**、key VID 0。ZCAM 地址用主线同一套 CRC16（选择子 0，key 即上述 8 字节）可复现 0x7f。

主线上的后续两项检查（同日）：

1. PC 10 + 那条 MAC 表项时，mac 发来的帧在路由器任何 netdev（`lan1`…`lan4`、`lan-cpu0`、`eth0`）上都抓不到，计数也不涨——是硬件里丢了（或转发到 CPU 以外的地方），不是 DSA 标签解析丢的。推测：源 MAC 命中后，目的 MAC（路由器自己）未被识别为"本机"，帧按 L2 处理被泛洪/丢弃。厂商一定在别处标记了本机 MAC，尚未找到；再加上全部 56 个不同寄存器（含 0x8340 路由器 MAC）、SDT 0 全表，仍然不通。
2. 在主线上复现"中途切换"：PC 561 下先让上行 PPPoE 流以厂商格式（multi + IKEY、action 0、`vlan_action` 0、`wan_mode` 1）进入硬件，再把 mem 5 切到 PC 10——流量停止，没有 `0x8864` 数据帧，与第 1 项一致：PC 10 在到达快表之前就把帧丢了。

所以剩下的问题集中在 PC 10 的 L2 阶段：除 SDT 9 主机表项外，它还需要某个"本机 MAC/路由接口"配置。下一步可选：(a) 在厂商机上用同样的中途切换 + 逐项删除，找出这项配置（还需往返刷机）；(b) 分析 `np.ko` 中 L3 接口/路由 MAC 相关的设置函数（`tm_*`、`zte_api_sw_*`）。

#### 1.5.1 反汇编补充（同日）

- **本机 MAC**：`switch.ko` `sw_mac_add_cpu_macaddress()`（8 槽）→ `np.ko` `tm_onu_mac_addr_set()` → `spa_set_Onu_MacAddr()` → `spa_local_onu_mac_set()`，只写 ONU 寄存器表里 ID 4/5（低 32 位/高 16 位，按槽号索引），第 0 槽即 SPA 0x8340/0x8344；同时 `wanid_set_mac()`。主线已手工写过第 0 槽，没有其他有效位。
- **主机 MAC 下发**：厂商网桥学到 FDB 时调用 `switch.ko` `br_add_fdb_mac(mac, hashnode, devname)` → `get_sw_port_from_devname()` → `zte_api_sw_mac_add_port_macaddress_hashnode(port, mac, 0, 1, …)`：类型 0xf，VID 0（除非全局开关），`set_mac_rtlmt_info()` 按 MAC 哈希分配限速槽 → `tm_mactable_add_set()` → `mac_table_add()` → `se_hash_table_set(9, …)`。
- **SDT 9 字段名**（`mac_table_entry_print`）：mac、vlan、phyport、status、smac_ctl、dmac_ctl、age idex、smac/dmac_bucket、smac/dmac_rt（10 位索引 +0x414/+0x814）、type、gre_id、fwd mode。存储的 56 位响应 = 读出布局右移 8 位，老化索引在 bit 37 起。按此解厂商那条：phyport **62**（= mac 所在 lan1/p7 的 S-VLAN 62）、VID 0、rt 976、bucket/ctl/fwd 全 0。把 rt 改成 0 后在主线上仍然不通。
- **丢在哪里**：PC 10 + 该表项时，mac 的 300 个 ping 计入 RED `in drop cnt`（TM 0x20208）、`out drop cnt`（0x20214）和 DMA `send wrdrop cmd`（0x38110），CPU 接收计数（IDM 0x280250）为 0；PC 561 时相反。即帧被转进了某个 TM 队列后被 RED 丢弃——目的队列在主线上未配置。不带表项的 PC 10 也有少量帧走这条路。

### 1.6 单步跟踪 PC 10：硬件加 PPPoE 头已跑通，但有随机崩溃（2026-10-15）

**工具。** `port/mainline/ppu-trace/ppu-trace.c`：用户态 PPU 断点/单步跟踪器（寄存器布局沿用以前 RSS 工作的 `rx-ppu-trace.c`）。主线经实验 debugfs `zx279133-pps`（PPS 窗口的原始 32 位读写）访问，厂商机可经 `/dev/mem`。`pc=N match=OFF:HEX,...` 在入口打断点、挑出匹配的帧单步到结束，输出每步的 PC、指令和 128 个字的变化（w0–63 本地包字，w64–95 = r0–r31，w96–127 = 查表响应）；`dump=PC count=N` 读指令 SRAM；`status` 只读停住掩码和各 ME 的 PC。`trace-diff.py` 压缩路径并给出两份跟踪第一次分叉处的寄存器差异。

**找到的两处缺失配置：**

1. **59–62 号逻辑端口的协议表。** PC 10 剥 S-tag 后用 VID（mac 所在 lan1 是 62）当端口号，按 `(端口<<7)+原因` 查协议表（PC 1548/1876 的 `0018061800400533`）。主线只写了 0/5/6/10 号端口，62 号全是 0 → 动作 0 → 本机流量走转发路径被 RED 丢掉（即 §1.5.1 的"LAN 全死"）。把 5 号端口的 16 块（SMMU0 块 21346+i）复制到 62 号（块 22258+i）后，PC 10 + SDT 9 主机表项下 ping 100/100，本地帧走 1893 队列消费 → CPU，与厂商的"in idm fwd"一致。
2. **LAN 入口流表项要设 `wan_mode`（byte 29 bit 1），并且不再剥 tag。** PC 10 已经剥掉 S-tag，表项保持 `vlan_action` 0，长度变化 PPPoE 为 4（+8 字节）、IPoE 为 0。没设 `wan_mode` 的 IPoE 上行帧在 PC 160 处 r15（出口端口）被清零，出口字 w5 的端口位变 0，帧发不出去。设了以后出口字与 PC 561 相同（`00408418`）。PPPoE push 用厂商格式：multi + IKEY、action 0。

**结果（PC 10 + 主机表项 + 62 号协议表 + 上述表项格式）：** IPoE 上/下 2350/2311、PPPoE 上/下 2326/2338 Mbit/s，CPU 空闲。笔记本抓到的上行帧是 `0x8864` 会话帧，PPP 协议 0x0021，PPPoE 长度、TTL、SNAT、校验和都对。PPPoE push 用 WANID 1 也正常。

**未解决：PC 10 下双向并发高负载时内核随机崩溃。**

- 现象：IPoE（或 IPoE+PPPoE）`--bidir` 同时跑，几轮之内（有时第 1 轮）路由器复位。多数没有任何内核输出；一次打出"跳到地址 0 执行"的 Oops，打印时又在内核地址上二次出错，即内核内存被破坏。CPU 先停，硬件转发还会持续几秒，约 10 秒后看门狗复位。
- 只上传、只下载各 6 轮都稳定；PC 561 下同样 4.4 Gbit/s 的 IPoE 双向 7 轮稳定；所以是"PC 10 + 双向"。
- 已排除：rx_hash 挂钩（`zx279133.rx_hash=0` 照样崩）、流统计（LAN 表项不开统计照样崩）、MAC 表项的 rt（rt=0 照样崩）、62 号协议表里的动作 0 项（全改动作 1 照样崩）、WANID 0/1、ZCAM 手写槽与流表分配冲突（已让分配器避开）、广播/组播洪泛（2 分钟、近 400 万广播帧不崩）。
- 软锁死检测（4 秒阈值）不报；面包屑（驱动各入口出口直接写 UART，`zx279133-crumbs`）显示出事时驱动函数都已返回。200 MB 金丝雀页（`zx279133-canary`，约占空闲内存 2/3）直到崩溃前 2 秒都没被改写，所以不像 DMA 随机乱写，更像写进了正在使用的页，或 NP 侧的缓冲/队列记账出错。
- PC 10 与 PC 561 入口字的差别：w11（PC 10 为 `f800003e`，即源逻辑端口 62；PC 561 为 `14000000`）、w1/w2/w10/w17（tag 偏移）。下一步怀疑 TM/BMU 对源端口 62 的配置（厂商 RED 间接表在 q376–400 的参数与主线不同），需要回厂商机导出 59–62 号端口的协议表和 TM/RED/BMU 相关配置对比。

**实验开关（都在实验内核里，提交前要删）：** np.c `zx279133-pps`、`zx279133-crumbs`、`zx279133-canary`，面包屑调用分布在 idm/netdev/offload/rtl8372n；offload.c `zx279133-lan-pc10`、`zx279133-lan-nostat`，`zx279133-zcam` 写入现在会占用分配器的槽。开 PC 10 的脚本：复制 62 号协议表、写 ZCAM 0/0/7f 主机表项、`echo "5 0 8015" > zx279133-spa`。

#### 1.6.1 崩溃的真正原因：kexec，不是 PC 10（2026-10-15）

- 回厂商机对照（slot 1 第 11 次重写）：厂商 59–62 号端口协议表与复制的 LAN 表只差原因码 30/33 的 `ppsnum` 位；在厂商机上用 ppu-trace 跟踪 PC 10，发现 **SDT 50 按逻辑端口索引**（PC 2713 的 `0018000640000002`，表号在 bits 34:29），厂商 59–62 号有表项（`1c8c2800 00000a38/a60/a88/ab0 2f235600/2fc36000/30636a00/31037400`），主线为空。补上后 PPPoE 上行、ACK、本机 ping 三类帧在 PPU 中的路径和最终描述字与厂商逐字一致。厂商机同样四路并发 8 轮稳定。
- 之后在主线上逐项排除：RED 间接表 232 项对齐、SPA 属性、IDM 空闲环 1–4、越界 RX 队列（扩到 64 队列检查）、强制 RX 复制、`DEBUG_PAGEALLOC`+`PAGE_POISONING`（只抓到一次 kcompactd 栈上定时器被写坏），都不是原因。
- 关键线索：同一次启动里先跑温和组合半小时，四路并发就不再崩；刚 kexec 起来就上高负载才崩（只 PPPoE 双向也会崩）。
- **冷启动验证**：把带实验代码的内核做成固件（slot 1 第 12 次重写，序号 82），冷启动、双核、开机约 1 分钟就打开 PC 10 + SDT 50 + 主机表项并立即四路并发：3 次冷启动共 20 轮全部稳定。
- 结论：崩溃是 kexec 测试环境造成的。kexec 时只复位 SDET/PON，IDM/BMU/QMG/PPU 的内部状态跨内核保留，加上 kexec 后单核运行。**PC 10 方案本身可行。** 以后凡是 NP 高负载/DMA 相关的稳定性测试，都要在冷启动的固件上做。

**PC 10 方案需要的配置（驱动正式实现时要做的）：**
1. LAN 内存 mem 5 起始 PC 10（`0x8015`）；
2. 59–62 号逻辑端口的协议表（复制 LAN 表，原因码 30/33 按厂商带 `ppsnum`）；
3. SDT 50 第 59–62 项（厂商值）；
4. 每个 LAN 主机在 SDT 9 有 MAC 表项（厂商由网桥 FDB 同步，键 = MAC + VID 0，phyport = 该主机所在端口的逻辑号）；
5. LAN 入口流表项：`vlan_action` 0、`wan_mode` 1、长度变化 PPPoE 4 / IPoE 0；PPPoE push 用 multi + IKEY、action 0。

#### 1.6.2 驱动实现（2026-10-15）

本机仓库工作区（未提交），实验代码已全部删除（debugfs 只保留 `zx279133-flows`、`zx279133-wanid`）：

- `zx279133-np.c` `zx279133_lan_port_bringup()`：mem 5 起始 PC 10（`0x8015`）；新增 `zx279133_lan_vport_bringup()`，给逻辑端口 59–62 写协议表（LAN 表，原因码 30/33 加 `ppsnum` 位）和 MF 扩展表项（SDT 50，厂商值）。
- `zx279133-offload.c`：LAN 入口表项不剥 tag、设 `wan_mode`、长度变化 PPPoE 4/IPoE 0；PPPoE push 改用 multi + IKEY、action 0，push 也分配计数器（顺带解决 §2 的计数问题）；新增 SDT 9 主机表项管理：下行流装入硬件后，按其目的 MAC（LAN 主机）和出口 VID（逻辑端口）装一条主机表项（ZCAM，CRC16 选择子 0、键 = MAC 低字节在前 + VID 0），按流引用计数，主机换口时就地改写；`zx279133-flows` 末尾列出主机表项。没有主机表项时上行帧走 CPU，功能不受影响。
- `zx279133-idm.c`：PC 10 交给 CPU 的 LAN 帧已去掉端口 tag，端口号从描述符 word 1 bits 21:16（59–62）取回，作为 DSA 的 VID；接收校验和判断同时接受这几个端口号。

kexec 功能验证（#42）：LAN ping/HTTP/DNS 正常；IPoE 上/下 2348/2353、PPPoE 上/下 2339/2338 Mbit/s，CPU 空闲；上行为 `0x8864` 会话帧；主机表项位于 0/0/127（与厂商一致）。固件 `release/candidates-pc10-20261015/sr1010-owrt.bin`（sha256 `e22c6ee5…`）。

冷启动验证（slot 1 第 13 次重写，序号 84，正式固件）：3 次冷启动，每次开机约 1 分钟起四路并发，有效 16 轮全部稳定、内核无告警（第 3 次的前 4 轮因测试环境未就绪没跑起来）。

lan2 验证（用户把 mac 换到 lan2）：网桥 FDB 正确记在 lan2；主机表项自动为逻辑端口 61；ping/LuCI 正常；IPoE 上/下 2349/2352、PPPoE 上/下 2339/2340 Mbit/s，上行为 `0x8864`，CPU 空闲。

PC 10 下 LAN 侧 RSS（发往路由器本机的流量同样经过 PC 10，并走 PC 1893/2216 的哈希挂钩）：mac → 路由器 iperf3 8 条流 2075 Mbit/s，分散到 6 个 LAN 队列，所有包都带硬件哈希和校验和；单条流三次分别完整落在队列 4、7、3，按流一致，不会乱序。

**防火墙重载时漏清硬件表项（原有问题，2026-10-15 修复）。** nf_tables 销毁旧 flowtable 时先对各设备 UNBIND、后释放 flowtable，释放时逐条下发的 DESTROY 已经到不了驱动；驱动原先只在"最后一个 block 解绑"时全清，而 fw4 重载是先绑新表再解旧表，列表永不为空，于是每次重载都遗留表项并泄漏其 WANID、主机表项引用、IKEY/老化/计数槽。修复：表项记录所属 flowtable（`flow_block`），某个 flowtable 的最后一个设备解绑时清掉属于它的表项（`zx279133_flow_entries_clear_block()`，删除逻辑与 DESTROY 共用 `zx279133_flow_entry_remove()`）。

"重建实验环境后第 1 轮慢/连不上"查明是测试环境：`pppoe-lab.sh up` 新建 `wanppp` 会重载网络和防火墙；PPPoE 会话若在测试开始后才建立，`ifup wanppp` 触发的防火墙重载会把正在卸载的流全部冲掉；有一次 netifd 没有重新拉起 pppd，脚本在路由器步骤失败，没给 mac 加路由。与驱动无关。

冷启动验证（slot 1 第 14 次重写，固件 sha256 `2ff12129…`）：2 次冷启动各 4 轮四路并发全部满速；满负载中途 `firewall reload`，四路流量不中断（约 1.1–1.2 Gbit/s），旧表项被清（18→8、20→8）；流量结束后只剩 mac 自身 ZeroTier UDP 连接的 8 条正常表项，无泄漏。

**UDP 与 IPv6（正式固件，冷启动后，2026-10-15）。** mac 在 lan2；单流 iperf3，UDP 包长 1400（IPv6 PPPoE 1380），目标速率 2.5 Gbit/s。测试中路由器 CPU 空闲，流都在硬件里。

| 场景 | 上行 | 下行 |
|---|---|---|
| IPv4 IPoE UDP | 2386 Mbit/s，丢 0% | 2368 Mbit/s，丢 0.058% |
| IPv4 PPPoE UDP | 2374 Mbit/s，丢 0% | 2362 Mbit/s，丢 0.018% |
| IPv6 IPoE TCP | 2319 | 2320 |
| IPv6 IPoE UDP | 2354，丢 0% | 2349，丢 0.03% |
| IPv6 PPPoE TCP | 2301 | 2303 |
| IPv6 PPPoE UDP | 2338，丢 0% | 2327，丢 0.0037% |

IPv6 PPPoE 上行帧：`0x8864`，PPP 协议 `0x0057`，hop limit 64→63，PPPoE 长度 1494。IPv6 测试环境为临时手工配置（`2001:db8::/32`：笔记本 `2001:db8:174::254`、目标 `2001:db8:200::1`/`2001:db8:100::1`；路由器 WAN `2001:db8:174::5`，需临时把 `net.ipv6.conf.eth0.disable_ipv6` 设为 0；br-lan `2001:db8:5::1`/`2001:db8:6::1`；mac 用两个源地址区分 IPoE/PPPoE 回程；两端 pppd 加 `+ipv6`），测试后已全部撤销。

LAN → 路由器本机（TCP 终结，CPU 受限）：收 单流 1617、8 流 2105 Mbit/s；发 单流 1225、8 流 1692 Mbit/s，多流时两核满载。

另修了 `pppoe-lab.sh down` 的问题：笔记本上的 `pkill -f <路径>` 会匹配到执行它的远程 shell 本身，导致后续清理（iperf3、`zxtest0`）没有执行；改为带方括号的匹配模式。

## 2. 问题 2：pop 计数不准，是 nf_flowtable 不拉硬件计数

对照（同一个实验内核，用开关把 push 的 action 设回 1）：

- debugfs `zx279133-flows` 读到 pop 方向的 SDT29 计数为 1,325,882 包 / 2.0 GB，就是全部下载数据：**硬件计数是准的**。
- conntrack 的回复方向却只有 1 个包。

原因在 `net/netfilter/nf_flow_table_offload.c` 的 `nf_flow_offload_stats()`：

```c
delta = nf_flow_timeout_delta(flow->timeout);
if ((delta >= (9 * flow_offload_get_timeout(flow)) / 10))
	return;
```

它只在流的剩余超时不足 90% 时才排一次拉取计数的工作。两个方向都在硬件时，软件不会刷新超时，约每 3 秒（TCP 默认 30 秒的 10%）拉一次。但 push 用 action 1 时，上行报文每个都走软件 flowtable，`flow_offload_refresh()` 每秒刷新超时，门槛永远达不到，**整条连接的硬件计数一次都不会被拉取**，连接结束时也不会补拉。

实验 1 中 push 用 action 0、两个方向都在硬件，conntrack 的回复方向最终是 1,438,680 包 / 2.18 GB，与 iperf 一致。

所以：

- 问题 1 解决，问题 2 就随之消失。
- 问题 1 解决之前，只要连接有一个方向在软件里，这个现象就会存在，不论驱动怎么写。要单独修，需要改 nf_flowtable 核心：给 `struct flow_offload` 加一个"上次拉取计数"的时间戳，按时间而不是按剩余超时来限流。这是通用改动，会影响所有用 flowtable 硬件卸载的驱动。

## 3. 实验中顺带确认的事

- **新增的 `/sys/kernel/debug/zx279133-flows`**（实验内核）：每条硬件流一行，包括 key、方向、SDT14/SDT43、ZCAM 或 DDR 位置、WANID、age 索引，以及直接读出的 SDT29 包数和字节数。值得保留。
- push 改用 action 0 并分配计数器后，push 方向的 SDT29 计数正常累加（115 万包 / 1.74 GB），说明 SDT14 整行表项的计数器本身没问题。

## 4. 现场状态（2026-10-14 收尾时）

- 测试机：flash 内核（slot 1，序号 58，`candidates-wanid-20261013`），双核。`wanppp` 的测试配置仍在，不接管默认路由；`pppoe-lab.sh down` 可以拆掉。
- 实验代码（action 0、push 分配计数器、`wan_mode`、debugfs 开关、WANID 覆盖）已全部撤掉：push 恢复 action 1，注释改为如实说明"action 1 交给 CPU，action 0 硬件转发但不加 PPPoE 头"。保留了 `zx279133-flows` debugfs 文件。本机仓库和编译机两棵树一致，W=1 和 checkpatch 都通过。
