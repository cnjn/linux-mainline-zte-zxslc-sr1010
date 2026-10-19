# 组播路由与硬件加速（调查中，2026-10-16 / 10-18）

## 主线软件组播路由（已跑通）

- 内核已有 `IP_MROUTE`/`IPV6_MROUTE`/`BRIDGE_IGMP_SNOOPING`。装 `igmpproxy`（另编了 `smcroute`，两者不能同时运行，smcroute 已 disable）。apk 在编译机 `bin/packages/aarch64_cortex-a53/packages/`，未进固件。
- 路由器配置（UCI，在 overlay 里）：`igmpproxy.@phyint[0].network=wan2`，altnet `192.168.175.0/24`、`10.64.0.0/16`；`network.@device[0].igmp_snooping=1`（br-lan）。转发规则由 igmpproxy 通过 procd 交给 fw4，要 `reload_config` 或重载防火墙才生效。
- 测试工具：笔记本 `/tmp/mcsend GROUP PORT SRCADDR MBPS SECS`（带序号），mac `/tmp/mcrecv GROUP PORT IF IFADDR SECS`（按网卡加入组，统计丢包和重复包）。源码在 scratchpad `mc/`。**macOS 的“本地网络”隐私限制：经 ssh 起的普通进程收不到组播，要 `sudo` 运行接收端。**
- 结果（mac en8 在 lan2，笔记本在 lan4，第二 WAN IPoE）：
  - (S,G) 正确建立；lan2 加入组时，未加入的 lan1 一个包都收不到（侦听剪枝正常，MDB 已同步到交换芯片）。
  - 基线：50 Mbit/s 时 CPU 29%，100 Mbit/s 时 55–60%，约 250 Mbit/s（2.4 万包/秒）时一个核跑满，路由器反应迟钝。
  - 注意：满载时路由器上的 shell 会卡住，“采样时 CPU 空闲”是采样命令推迟到推流结束后才执行的假象。

## 原厂（第二 WAN = lan4 上的 PPPoE）

- 原厂内核组播路由（cspd 里的 IGMP 代理）默认就在运行：VIF 为 br0 和 ppp0。
- **是软件转发**（FFE 快速通道，绕过 ipmr，ipmr 只计到最初几十个包）。400 Mbit/s 推流时：
  - CPU 侧 `pon` 收 3.8 万包/秒，`br0`、`eth3`（lan2）各发 3.8 万包/秒；
  - CPU：100 Mbit/s 10–13%，400 Mbit/s 15–30%，700 Mbit/s 26%（丢 0.4–0.8%），1000 Mbit/s 丢 26%。上限约 750 Mbit/s。
  - 用户在网页上打开组播设置后 CPU 略降，但仍是每包经过 CPU。
- 快表（SDT 43/14）和 SDT 10（IPv4 组播表）在推流时都没有组播表项；端口组（eRAM 3，下标 1024 起）全 0。
- 先前用 busybox `top -b -n 1` 看到的“100% idle”不准，要用 /proc/stat 前后两次采样。
- 在原厂上用 ppu-trace 跟踪又一次把转发卡死，要重启。

## 原厂（WAN 口 = eth0 上的 PPPoE，2026-10-18）

- 网页上只有组播开关，上游由 `IGMPWan.tWanCID` 决定，当时绑在 `DEV.IP.IF3`（PPPoE_2，即 WAN2）。WAN 口那条连接是 `DEV.IP.IF2`（名为“PPPoE”）。
  - 改法：串口执行 `sendcmd 1 DB set IGMPWan 0 tWanCID DEV.IP.IF2; sendcmd 1 DB save`，然后在网页上把 IGMP 代理关掉再打开（不用重启）。之后 ppp0 出现在 `/proc/net/ip_mr_vif`。
  - 正常 `reboot` 或 `reboot -f` 都会让这个值变回 IF3；只有关开代理这一步能生效。
- mac（macOS 26.5）：en0 拔掉后，加入组时内核计数里“报告已发送”在涨，但 en8 上抓不到报告，原厂代理也收不到。改用 `/tmp/igmprep GROUP IFADDR SECS`（原始套接字发 IGMPv2 报告）后，原厂立刻装上 (*,G) 路由。
- **结果：仍是软件转发**（每个包都经过 CPU，ipmr 只计 37 个包，其余走 FFE），但效率高、不丢包：
  - 100 Mbit/s：CPU 8.9%；
  - 400 Mbit/s：13.8%；
  - 700 Mbit/s：26.2%，只丢 1 个包；
  - 包速率：pon 收 = br0/eth3 发 = 实际流量的包速率。
- 结论：原厂在第二 WAN 和主 WAN 上都没有用硬件转发路由组播，靠的是一条精简的内核软件快速通道。主线软件路径（100 Mbit/s 时 55–60%）比它慢 6 倍左右。

## 原厂 np.ko 里的组播硬件结构（静态分析，未在运行中见到使用）

- 路由组播：FFE → switch.ko `npu_drv_create_flow` → `sw_add_igmp_addr_e8v4(出口设备)` → np.ko `tm_ipv4table_add_set` → `mul_table_set`。
- SDT 10 是 multi 哈希表（表号 0，选择子 0x00，表项尾字节应为 0xc0），`se_hash_multi_table_set(10, …)`。
  - key 10 字节：源地址、组地址、12 位 VLAN，带掩码，源为 0 即 (*,G)。
  - 响应 8 字节：端口数 10 位（u16 @2 的 bit 6–15）、端口组起始下标 16 位（u32 @4 的 bit 9–24）、有源地址标志（字节 7 bit 2）。
- 端口组：`group_multi_set`，eRAM 表 3，从 0x400 起每组 `maxports` 项，每项为一个转换后的端口号（`comm_change_portindex_in`）。由 NP 复制，每个出口端口一份。
- 调试接口：`/sys/devices/platform/np/table_debug/`，包括 `hash_table_dump`（9 MAC、10 组播 v4、11 组播 v6、14、43）、`mulhash_table_set`、`groupid_table_get/set`、`v4_table`，输出都在 dmesg。
- 主线已按原厂建了 SDT 10 和 eRAM 3。但主线上跟踪从 lan4（WAN 类型）进 PC 10 的组播帧，没有查 SDT 10，最后送给 CPU。

## 下一步

- 原厂在 WAN 口上同样不走硬件（2026-10-18 已测）。
- 主线可选方向：
  - (a) 优化软件路径，向原厂 FFE 的水平看齐。先剖析每包开销：netfilter/conntrack、ipmr、bridge 复制、DSA、单队列 NAPI。
  - (b) 超出原厂，做硬件：用快表 5 元组表项（单出口），或 SDT 10 + eRAM 3 复制。微码路径未验证，原厂也没用过。

## 主线软件路径剖析（2026-10-18，WAN 口 PPPoE，固件 f66c96d8）

**测量方法**
- 脚本：scratchpad `mcbench.sh MBPS SECS`。笔记本经 PPPoE（10.64.0.1）发 239.1.1.1:5000，1316 字节载荷；mac en8（lan2）用 `igmprep` 加 `mcrecv` 接收。
- `/proc/stat` 是 HZ=100 的 tick 计时，但和“最低优先级吃空闲”计数（`soak`，SCHED_IDLE）结果一致。
- 不跑 soak 时，`perf -a` 会把大部分样本算进 `default_idle_call`，严重低估。要和 soak 同跑，perf 的分布才可信。
- soak 本身在高负载下会造成丢包（ksoftirqd 抢不过来），所以测丢包上限时不跑 soak。

**基线**
- 无丢包上限约 200 Mbit/s；300 Mbit/s 起单核跑满，约 2.1 万包/秒封顶。原厂约 700 Mbit/s（约 6.6 万包/秒）。
- 100 Mbit/s 时收包核约 57% 时间在内核，相当于每包约 60 µs。
- 按调用链分（占该核时间）：

| 部分 | 占比 |
|---|---|
| `ip_mr_input` 合计 | 33.6% |
| 其中 `ip_mr_forward` | 24% |
| 其中 `ip_local_deliver` | 8.7% |
| `nft_do_chain` 合计 | 14.5% |
| 驱动收包 | 10% |
| 网桥、DSA、驱动发包 | 7–9% |
| ipmr 每包新建再释放输出路由（`ip_route_output`、`dst_destroy`、RCU） | 约 5% |
| dae 的 BPF | 约 1.3% |

**为什么有 `ip_local_deliver`**：igmpproxy 在上游口（pppoe-wanppp）上 `IP_ADD_MEMBERSHIP` 加入了组，内核因此把每个包复制一份走 INPUT 链（`ip maddr` 能看到 239.1.1.1）。

**零代码修剪（逐项测上限）**
- 单独 notrack：2.5 万包/秒（+20%）。
- 在 fw4 自己的 forward/input 链最前面插提前放行/丢弃：2.8 万包/秒（约 295 Mbit/s，+33%）。
  - 另建一张表、在里面 accept 没用：nft 里 accept 只结束这一条基础链，fw4 的链还会再走一遍。
- RPS：
  - 在 pppoe-wanppp 上不生效。驱动用 `skb_record_rx_queue` 记的是 eth0 的 1 号队列，PPPoE 解封装后这个记号还在包上；ppp 设备只有 1 个队列，`get_rps_cpu` 遇到越界队列号就静默跳过 RPS。这是个可以另外修的小问题。
  - 在 eth0 上开 RPS：协议栈挪到另一核，收包核只剩约 18%，吞吐只提高约 5%。
- 结论：靠修剪最多到约 1.3 倍。要追上原厂，需要像 FFE 那样绕过 IP 层、netfilter 和 ipmr 的快速通道。

**测试环境事故**：主线开机后，lan1 上的 Z506 子路由没了控制器，自己以 192.168.5.1 的身份发 60 秒的 DHCP 租约。主路由的 dnsmasq 开机时检测到别的 DHCP 服务器，因此不提供 DHCP，mac 的地址不断变化。用户拔掉 Z506 后，重启 dnsmasq 就恢复了。

## mcfast：tc BPF 组播快速通道（2026-10-18，已实现并实测）

**思路**：和原厂 FFE 一样绕过协议栈，但不改内核、不依赖本 SoC。只加速内核组播路由表（MFC）里已经解析的 (S,G)。MFC 由 igmpproxy 或 smcroute 设置，所以“转发什么”仍由路由守护进程和防火墙决定。

**天花板验证（零代码）**：在 eth0 的 tc ingress 挂一个最小的 BPF 程序，把 IPoE 组播直接 `bpf_redirect` 到 br-lan（网桥照常按 MDB 剪枝）。900 Mbit/s（8.5 万包/秒）无丢包，单核 95%。

**实现**：源码在 `port/mainline/mcfast/`，是一个 ImmortalWrt 包，编译机上放在 `immortalwrt/package/network/services/mcfast/`。

- `src/mcfast-bpf.c`：两个 tcx ingress 程序。
  - `mcfast_eth`：用于以太入口（IPoE）。
  - `mcfast_ip`：用于无二层头的入口（PPPoE 会话设备），先用 `bpf_skb_change_head` 补一个以太头。
  - 按 (入口 ifindex, S, G) 查哈希表，查不到再按 (S=0, G) 查。
  - 做 `ip_mr_forward` 该做的事：TTL 门限、MTU 检查、TTL 减一并修正校验和、组播目的 MAC、按出口写源 MAC。
  - 然后对每个出口 `bpf_clone_redirect`，最后一个用 `bpf_redirect`。出口一般是 br-lan，由网桥做 IGMP 侦听剪枝。
  - 不匹配的包返回 `TC_ACT_UNSPEC`（tcx 的 NEXT），dae 等后面的程序照常运行。
- `src/mcfast.c` 守护进程（依赖 libbpf）：
  - 用 netlink dump `RTNL_FAMILY_IPMR` 取 MFC。
  - 监听 `RTMGRP_IPV4_MROUTE` 和 `RTMGRP_LINK` 事件，另外每秒全量同步一次。
  - 用 `BPF_F_BEFORE` 把 tcx 程序挂到正在使用的入口设备上，挂在 dae 前面；不再使用时摘掉。PPPoE 重拨换了 ifindex 也会自动重挂。
  - 状态写在 `/var/run/mcfast.status`，每条显示包数和字节数。
- 以下情况留给内核处理：
  - 入口既不是以太设备也不是 PPP 设备；
  - 出口里有非以太设备；
  - 出口为 0 个或超过 4 个；
  - 入口上除路由守护进程自己那一份之外还有本机成员（`/proc/net/igmp` 的 Users > 1，例如 udpxy）。
- 已知差异：被加速的 (S,G) 不再经过 netfilter，`ip mroute` 的计数只到快速通道接管为止。igmpproxy 的路由老化只看 IGMP 报告，不受影响。

**实测**（mac en8 在 lan2，1316 字节载荷，不跑 soak）：

| | 原主线 | mcfast | 原厂 |
|---|---|---|---|
| PPPoE 无丢包上限 | 约 200 Mbit/s | 700 Mbit/s（单核 94%） | 约 700 Mbit/s（偶有丢包） |
| PPPoE 饱和点 | 2.1 万包/秒 | 7.9 万包/秒（约 830 Mbit/s） | — |
| PPPoE 100 Mbit/s 时收包核占用 | 54% | 22% | — |
| IPoE 无丢包上限 | — | 900 Mbit/s（87%） | — |
| IPoE 饱和点 | — | 10.1 万包/秒（约 1.07 Gbit/s） | — |

- 300 Mbit/s 连续 330 秒：940 万个包零丢失、零重复。MFC 条目全程稳定，内核只转发了 MFC 装上之前的约 360 个包。
- PPPoE 重拨（ifindex 9 → 13）后自动重挂，接着加速。
- 接收端离开后，igmpproxy 删掉 MFC，mcfast 同步删除条目。
- 单播回归不受影响：PPPoE 下载 1934、上传 2353 Mbit/s。
- 网桥剪枝已验证：100 Mbit/s 推流时，5 秒内 `br_multicast_flood` 被调用约 4.75 万次（等于包速率），`br_flood` 只有 12 次（背景流量）。mcfast 开、关两次结果相同，说明重定向进 br-lan 的包照样按 MDB 转发，不会泛洪到所有口。用的是 perf kprobe，因为当时只有 lan2 有链路。
- 尚未测试：
  - 多个出口的扇出（先 clone，再在共享的 skb 上改源 MAC）；
  - (*,G) 路由；
  - IPoE 上游时程序挂在 eth0 上，此时的单播回归；
  - 服务开机自启（`/etc/init.d/mcfast enabled` 已确认，但还没重启验证）。

**待定**
- 是否把 `mcfast`（和 `igmpproxy`）编进固件默认包。目前是 `=m`，测试路由器上是手动 apk 安装的。
- IPv6（ip6mr）可以照同样的思路扩展。
- PPPoE 还可以在 eth0 上用 XDP 剥 PPPoE 头，省掉内核的 PPPoE 解封装和 backlog 这一跳，性能能再接近 IPoE。暂不需要。

## omcproxy、mcfast 加 IPv6，以及 fw4 卸载组播的问题（2026-10-19）

**组播代理改用 omcproxy**
- omcproxy 是 OpenWrt 官方的组播代理，同时支持 IGMPv3 和 MLDv2，也就是 IPv4 和 IPv6 都能管，带 LuCI 页面。
- 它和 igmpproxy 一样，在上游口用 socket 加入组（本机成员数为 1），写组播路由时下游的 TTL 门限设为 1。所以 mcfast 判断“本机还有没有其他接收者”的阈值不用改。
- omcproxy 严格按 RFC 要求，非查询类的 IGMP/MLD 报文必须带 Router Alert，否则记一条 `ignoring invalid IGMP-message` 后丢弃。真实设备都会带。
- 测试工具的问题：原来的 `igmprep` 不带 Router Alert；而 macOS 自己发出的 IP 包只要带选项就会被悄悄丢掉（`IP_OPTIONS` 和自己构造 IP 头都一样）。改用 `mcrep GROUP IFNAME SECS`：通过 `/dev/bpf` 直接写以太网帧，IPv4 发带 RA 的 IGMPv2 报告，IPv6 发带逐跳选项头和 RA 的 MLDv1 报告。

**mcfast 支持 IPv6**
- 新增第二张表 `mcfast_routes6`，键是入口加 128 位的源地址和组地址。
- hop limit 直接减一（IPv6 头没有校验和）；组播 MAC 为 `33:33` 加组地址后 4 字节；只转发作用域大于 link-local 的组。
- 守护进程同时读 `RTNL_FAMILY_IPMR` 和 `RTNL_FAMILY_IP6MR` 两张组播路由表，监听 `RTMGRP_IPV6_MROUTE`；本机成员检查读 `/proc/net/igmp6`。
- 测试环境：笔记本 ppp0 和路由器 pppoe-wanppp 上临时加 `2001:db8:64::1/::2`；组 `ff0e::239:1`；mac 用 `mcrecv6` 接收、`mcrep` 发报告。

| WAN 口 PPPoE + mcfast | IPv4 | IPv6 |
|---|---|---|
| 300 Mbit/s | 零丢包，收包核 50% | 零丢包，51% |
| 700 Mbit/s | 零丢包，94% | 丢 0.24%，95% |
| 饱和点 | 约 7.9 万包/秒 | 约 7.7 万包/秒 |

**fw4 会把 IPv6 组播流放进软件 flowtable**
- 现象：路由条目在推流中途建立时，eth0 每秒收 2.85 万包，pppoe-wanppp 收 0；mcfast 和 ip6mr 的计数都停在个位数，而 mac 照样收全。
- 原因：
  1. 最初几个包走内核 ip6mr 时经过 fw4 的 forward 链；
  2. 链首规则是 `meta l4proto { tcp, udp } flow offload @ft`；
  3. 6.18 的 `nft_flow_offload` 不排除组播，也不要求连接被应答，于是这条流被卸载，conntrack 里显示为 `[UNREPLIED] … [OFFLOAD]`；
  4. 之后的包在 eth0 入口由 flowtable 剥掉 PPPoE 头，直接送进 br-lan。
- 后果：
  - 只有一个出口（ip6mr 最初用的那个）；
  - 接收者离开、路由被删之后，只要源一直在发，流就一直有效；
  - TTL 门限、多个下游口、mcfast 都被绕过。
  - 网桥仍按 MLD 剪枝，所以测试里包没有发错地方，但语义不对。
- IPv4 组播在同样的规则下没有被卸载，原因未深究。
- `meta pkttype host` 挡不住：PPPoE 内层包的 pkttype 继承自外层的单播帧。
- 修复：ImmortalWrt 的 `firewall4/patches/003-fw4-no-multicast-offload.patch`，把规则拆成两条：
  - `meta nfproto ipv4 … ip daddr != 224.0.0.0/4 flow offload @ft`
  - `meta nfproto ipv6 … ip6 daddr != ff00::/8 flow offload @ft`
- 修复后验证：
  - 组播流不再被卸载，mcfast 在最初约 400 个包之后接管，300 Mbit/s 零丢包；
  - 单播 PPPoE 下载仍是 `HW_OFFLOAD`，1934 Mbit/s。

**同批进固件的其他东西**
- mwan3、luci-app-syncdial。
- 首次启动脚本 `99-zx279133-mwan3`：mwan3 配置还是出厂原样时，关掉它默认的 `wan` 条目，避免它 ping 不通公共 DNS 时把上网流量判为不可达。
- 6.18 内核把 ip_tables/ip6_tables 移到了 `NETFILTER_XTABLES_LEGACY` 开关后面，而 OpenWrt 的 `kmod-nf-ipt` 还需要这两个模块（mwan3 通过 iptables-nft 用到），所以在 `config-6.18` 里打开这个开关。
