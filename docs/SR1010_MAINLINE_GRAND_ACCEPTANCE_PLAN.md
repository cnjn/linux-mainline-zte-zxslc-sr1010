# SR1010 当前主线能力大验收方案

日期：2026-09-01

适用基线：

- 外层仓库：`639322ad925eac62e2b8305e07a98e47f2cafa68`
- Linux 子仓库：`linux-6.18.38`，`5336656ce7a3e7a8c18f8346284448b7ed5a25a7`
- 当前 FIT：`out/sr1010-zxdbg.itb`
- 当前 FIT SHA-256：`952ab576f722ee2655d2f0dba6abb35ea80007aab8e8f32e8b0f4f3fa6ff361a`
- 当前 `vmlinux`：`out/kernel/vmlinux`
- 当前 `vmlinux` SHA-256：`82a0ce6c4786225da8cf806ede193fdb57c21a3ba7da4bc4a421e8c671ac81b5`
- 当前实际 `.config`：`out/kernel/.config`
- 当前实际 `.config` SHA-256：`12b868472fdf61bf9a6ba6e704a35eedb30eb52dc85b05d38b37e97393cd7435`

这是一份“冻结现有功能、不增加新功能”的 release-qualification 方案。测试夹具和采集工具可以补齐，但不能把测试工具缺失伪装成能力通过。

## 1. 总原则

### 1.1 验收结论只有四种

|结论|含义|
|---|---|
|PASS|所有规定输入、输出、硬件证据、清理和重复性要求均满足|
|FAIL|能力、性能、资源生命周期、恢复或内核健康任一硬门槛失败|
|BLOCKED|缺外部设备、测试程序或可观测接口，无法得出结论|
|NOT APPLICABLE|当前驱动明确不支持，且本轮只验证正确软件 fallback/正确拒绝|

不能使用“基本通过”“大概没问题”“功能看起来正常”。BLOCKED 不是 PASS。

### 1.2 每个网络硬件 offload 必须同时有四类证据

1. **Linux 控制面**：`skip_sw`、`in_hw`、`in_hw_count 1` 或 conntrack `[HW_OFFLOAD]`。
2. **硬件活动**：SDT29 counter、age、DDR comparator、目标 bucket/ZCAM 或其他直接硬件计数。
3. **端点事实**：发送端和接收端 packet/byte，以及必要时 payload hash。
4. **CPU 事实**：`/proc/stat`、`/proc/softirqs`、IRQ 增量；不能只看吞吐。

只出现 `[HW_OFFLOAD]`，最多证明规则编程，不能单独判硬件命中 PASS。

### 1.3 安全边界

- 所有板上改动只存在于 RAM。
- 禁止 `saveenv`。
- 禁止 NAND/SPI-NAND erase/write。
- 禁止修改 U-Boot 环境。
- SFC/MTD 只允许读和重复 hash。
- 不静默修改 Mac `en8=192.168.1.100`、`en0=192.168.10.100` 等手工地址。
- HVF/vmnet fixture 若要求移除冲突地址，必须在 manifest 中记录原值、明确执行并在结束后恢复。
- 不把 QEMU/USB passthrough 吞吐当作路由器 2.5G 上限。
- 每个会改变 link、nft、tc、PPPoE、EEE、XDP 的用例必须有清理动作和失败 trap。

## 2. 验收拓扑

### 2.1 普通以太网拓扑

|角色|接口|地址|
|---|---|---|
|SR1010 WAN|`eth0`|`192.168.1.1/24`|
|macOS WAN peer|`en8`|`192.168.1.100/24`|
|SR1010 LAN|当前被测 `lan1..lan4`|`192.168.5.1/24`|
|Windows LAN peer|2.5G NIC|`192.168.5.100/24`|

IPv6 routed topology：

|角色|地址|
|---|---|
|SR1010 WAN|`fd00:1::1/64`|
|macOS WAN peer|`fd00:1::100/64`|
|SR1010 LAN|`fd00:5::1/64`|
|Windows LAN peer|`fd00:5::100/64`|

### 2.2 PPPoE 拓扑

- 使用已验证的 HVF `qemu-system-aarch64` Alpine standard fixture。
- `virtio-net-pci` 通过 `vmnet-bridged` 接 `en8`。
- 只使用一个活动 PPPoE SID/context，符合当前驱动限制。
- IPv6 over PPPoE 使用双方显式 `/128` peer route。
- QEMU SSH 转发端口、镜像 hash、内核版本、pppd/rp-pppoe 版本必须进入 manifest。

### 2.3 四个 LAN 口

当前 `lan1` 证据最强，`lan2..lan4` 不能用 DTS fixed-link 代替实测。若只有一个 Windows 2.5G peer，应按 `lan1 → lan2 → lan3 → lan4` 移动同一根线，每次记录：

- 实际物理端口标签
- DSA netdev 名称
- link up/down 时间
- port MIB before/after
- peer NIC link/speed

## 3. 运行目录与证据格式

每次完整验收新建：

```text
out/acceptance/YYYYMMDD-HHMMSS/
├── manifest.txt
├── source/
├── serial/
├── board/
├── mac/
├── windows/
├── vm/
├── pcap/
├── cases/
└── FINAL-RESULTS.tsv
```

不得覆盖上一次结果。

### 3.1 manifest 必须记录

- 外层和 Linux commit
- `git status --short`
- 所有 source diff 的 SHA-256
- FIT、vmlinux、`.config`、DTB、PPU microcode hash
- `uname -a`
- U-Boot banner/version
- Mac、Windows、QEMU/Alpine 版本
- Mac/Windows 网卡型号、driver、firmware、link mode
- 所有静态 IP/MAC
- QEMU 启动命令
- 测试开始/结束时间和时区
- 实际物理接线

### 3.2 每个 case 的固定文件

```text
cases/G04-IPv4-NAT-UDP/
├── command.txt
├── board.stdout
├── board.stderr
├── mac.stdout
├── windows.stdout
├── before.state
├── during.state
├── after.state
├── dmesg.delta
├── counters.tsv
├── result.txt
└── cleanup.txt
```

`result.txt` 只能写 PASS/FAIL/BLOCKED/NOT APPLICABLE，并附一行原因。

### 3.3 固定状态快照

每个重要 case 前、中、后采集：

```sh
uname -a
ip -d link
ip addr
ip route
ip -6 route
bridge link
bridge vlan
bridge fdb show
tc -s qdisc show
tc -s filter show dev eth0 ingress
tc -s filter show dev lan1 ingress
nft list ruleset
cat /proc/net/nf_conntrack
cat /proc/net/dev
cat /proc/interrupts
cat /proc/softirqs
cat /proc/stat
cat /sys/class/watchdog/watchdog0/state
cat /sys/class/watchdog/watchdog1/state
```

另采集 `ethtool -S`、`ethtool -k/-g/-a/--show-eee`；当前 initramfs 没有完整 ethtool 时，应显式 staging 到 `/tmp`，并记录 binary hash。

## 4. 全局失败和停止条件

任一出现后立即停止当前 gate，保存证据，执行最小清理，不继续跑后续压力测试：

- kernel BUG、Oops、panic、WARNING
- hung task、RCU stall、soft lockup、hard lockup
- use-after-free、double free、page_pool/DMA 警告
- SMMU/IDM/PPU timeout
- WANID/PPPoE restore failure
- watchdog feeder 意外退出
- netdev 无法恢复 carrier/queue
- flowtable flush 后硬件表项不归零
- ZCAM/DDR bucket 删除后仍有效
- 资源无法回到 gate 起始基线
- 串口失联且 watchdog 未按预期恢复

基础 gate 失败时的依赖关系：

- G0/G1 失败：停止全部验收。
- G2 基础网卡失败：停止所有网络 gate。
- G3 软件慢路径失败：停止硬件 offload gate。
- G5 统计/清理失败：停止容量和长稳。
- 任一 cleanup 失败：必须重新启动并重新建立资源 baseline，不能直接进入下一项。

## 5. G0：源码、构建和制品门

目标：证明测试的不是未知/陈旧 artifact。

|ID|项目|PASS 条件|
|---|---|---|
|G0.1|外层和 Linux 状态|commit、dirty files 完整保存；Linux 子仓库无未知源码改动|
|G0.2|完整构建|`./port/mainline/build-zxdbg.sh` 成功|
|G0.3|W=1|ZTE/ZX279133 相关 driver 无新 warning|
|G0.4|whitespace|外层与 Linux `git diff --check` 均通过|
|G0.5|config gate|build script 中全部 required config 检查通过|
|G0.6|artifact manifest|FIT/vmlinux/config/DTB/microcode/test tools hash 全部写入 manifest|
|G0.7|FIT 内容|kernel、DTB、initramfs、PPU microcode、nft rules、pppd、xsk-zc、ptp-test 均来自本次 manifest|

当前工作树存在 generated outputs 和用户/IDA 改动。验收不能自动清理或覆盖这些内容；先记录，再决定使用现有 FIT 还是新构建 FIT。无论哪种，都以本轮 hash 为唯一身份。

## 6. G1：启动、复位和基础平台门

### 6.1 三种启动形态

|ID|启动形态|方法|目的|
|---|---|---|---|
|G1.1|首次装载|U-Boot shell：`tftpboot sr1010-zxdbg.itb; bootm`|装载并启动本轮 FIT|
|G1.2|direct boot|Linux `reboot -f` 后，U-Boot 只执行 `bootm`|验证没有本轮 U-Boot network init 帮助时 Linux 能恢复|
|G1.3|真实 power cycle|断电/上电后重新 TFTP 当前 FIT|验证真正冷硬件状态|

G1.2 至少连续 10 轮。G1.3 至少首尾各一轮。

### 6.2 启动 PASS 条件

- 两个 Cortex-A53 online。
- UART console 无乱码/丢失。
- watchdog0/1 feeder 均启动，sysfs state 均为 `active`。
- deferred probe 在规定时间内收敛。
- `eth0`、`lan-cpu0`、`lan1..lan4`、`ptp0`、hwmon、MTD/USB controller 等当前配置预期设备出现。
- RTL8372N 两个 loadable modules 正确加载、依赖正确。
- PPU microcode version 正确。
- eth0 最终 2.5G/full，flow control off。
- init 固定 CPU/IRQ affinity 成功，或失败被明确记录并判定是否影响后续可比性。
- dmesg 无全局失败条件。

## 7. G2：板级外设和只读安全验收

|ID|能力|方法|PASS 条件|
|---|---|---|---|
|G2.1|watchdog|确认两个 feeder PID 和 state；最后单独做一次受控 feeder-kill 重启|两个 watchdog 可触发并恢复；该用例只能放在单独 reboot round|
|G2.2|PVT/hwmon|读取 `temp1_input`、`in1_input` 10 次|数值可读、范围合理、无突跳/错误；不声称 thermal trip/cooling|
|G2.3|SFC/SPI-NAND|`/proc/mtd`、dmesg、只读 `dd` + SHA-256，重复三次|分区只读；三次 hash 相同；绝不执行 write/erase|
|G2.4|GPIO LED|记录原 brightness/trigger，白/红 LED 各切换一次并恢复|sysfs 和实物一致，恢复原状态|
|G2.5|reset key|initramfs 无产品 factory-reset daemon 时读取 input event，人工按一次|只产生预期 key event；不执行恢复出厂动作|
|G2.6|USB host|插入已知 USB storage，只读 mount/hash/unmount|枚举、读取、卸载成功，无 USB reset loop/rx error|
|G2.7|eFuse/NVMEM|只验证 probe 和 PVT ttrim consumer|当前无稳定 userspace raw eFuse ABI，不做 devmem 猜读|

缺 USB 盘、无法观察 LED/按键时，相应用例标 BLOCKED，不能标 PASS。

## 8. G3：WAN/LAN 基础网卡和 DSA 门

### 8.1 WAN 基础能力

|ID|项目|PASS 条件|
|---|---|---|
|G3.1|2.5G autoneg|每次 cold/direct boot 均恢复 2.5G/full；flow control off|
|G3.2|1G fallback|peer 强制/限制到 1G 后可 link、双向传输；恢复 2.5G|
|G3.3|100M/10M|peer 支持时逐级测试；不具备 fixture 则 BLOCKED|
|G3.4|live MAC change|down/up 与 running 状态分别变更 MAC，再恢复|WANID CPU MAC 同步，通信恢复，无旧 MAC 泄漏|
|G3.5|MTU|1500、1970 成功；1971 被拒绝|1970 双向大包无截断；超限正确失败|
|G3.6|ring geometry|`ethtool -g`|RX 2048、TX 1024，固定 geometry；set ring 不属于当前能力|
|G3.7|pause state|`ethtool -a` + 必要寄存器只读|pause 不广告，RX flow-control 未被误开|

### 8.2 四个 LAN 口逐口等价性

每个 `lan1..lan4` 独立执行：

1. link down baseline。
2. 插线后 2.5G/full carrier。
3. IPv4 ARP/ping 100 次零丢失。
4. 双向 UDP echo 1000 包，payload hash/sequence 正确。
5. 端口 MIB 只在正确物理 port 增长。
6. 拔线后 carrier 在规定时间内 down。
7. 重新插线后恢复通信。

只有四口全部通过，才能写“lan1..lan4 已验收”。

### 8.3 DSA bridge/VLAN/FDB/STP

|ID|项目|PASS 条件|
|---|---|---|
|G3.20|VLAN-unaware bridge|lan port 加入 br0，IPv4/IPv6 双向通|bridge/DSA 状态一致，流量正确|
|G3.21|VLAN-aware access|PVID 100，LAN peer 仍 untagged|VID100 生效，无 private DSA tag 泄漏到 peer|
|G3.22|VLAN add/del|增加、删除 VID，重复 20 轮|硬件/bridge 状态同步，无 stale membership|
|G3.23|FDB add/del/dump|static FDB + 动态学习|正确端口、VID、MAC；删除后不残留|
|G3.24|STP block/forward|切 block 后业务停止，切 forward 后恢复|无跨端口误转发|
|G3.25|软件 fallback|ICMP 通过 bridge/NAT|conntrack 不得错误出现 `[HW_OFFLOAD]`|

Tagged trunk、MDB、LAG、mirror、switch tc 不属于当前已实现范围，本轮只确认没有错误宣称。

## 9. G4：纯软件慢路径门

硬件 offload 前必须证明 Linux slow path 本身正确，否则后续结果无法归因。

|ID|项目|PASS 条件|
|---|---|---|
|G4.1|IPv4 route|不加载 flowtable，TCP/UDP/ICMP 双向|端到端正确，无 HW marker|
|G4.2|IPv4 SNAT software|普通 nft NAT，不使用 flow add|地址/端口转换正确|
|G4.3|IPv6 route|不加载 IPv6 flowtable，TCP/UDP/ICMPv6 双向|正确路由，无 HW marker|
|G4.4|bridge/VLAN slow path|VLAN-aware br0 下 ICMP/TCP|正确 forwarding|
|G4.5|fragment fallback|IPv4 fragment 通过或按规则拒绝|不得错误进入硬件，不得崩溃|

## 10. G5：IPv4 自动硬件 NAT 门

使用 [nft-flowtable.nft](/Volumes/code/zx279133/port/mainline/nat-acceptance/nft-flowtable.nft)；不得为 nft 用例额外安装手工 flower。

### 10.1 功能和自动学习

|ID|项目|PASS 条件|
|---|---|---|
|G5.1|UDP established|先单向，再产生真实回复|UNREPLIED 不提前 offload；回复后 `[HW_OFFLOAD]`|
|G5.2|TCP established|完整握手、双向 payload|双方向硬件表项，连接关闭后正常清理|
|G5.3|SNAT/port NAT|验证 wire-side tuple|地址、端口、checksum 均正确|
|G5.4|DNAT/reply direction|反向 tuple 和 Ethernet rewrite|双方向端点正确|
|G5.5|existing conntrack|先建连接再建 flowtable|旧连接不得错误迁入；新连接可 offload|

### 10.2 吞吐门槛

在固定 endpoint、包长、线程数下记录原始结果。硬门槛取“历史基线下降不超过 5%”和绝对下限两者较严格者。

|项目|绝对下限|
|---|---:|
|IPv4 UDP NAT 30 s，发送端|≥ 2.40 Gbit/s offered|
|IPv4 UDP NAT 30 s，接收端|≥ 2.30 Gbit/s received|
|端点 packet difference|≤ 0.05%|
|硬件路径 CPU busy|≤ 1%|
|非预期 softirq 增长|接近 0；任何明显持续增长必须调查|

同时保存 sender/receiver packet、byte、网卡 drop/error、router IRQ/softirq。

### 10.3 flush 和 fallback

在活动 TCP、活动 UDP 下分别：

1. `nft flush ruleset`。
2. 要求 `[HW_OFFLOAD]` 立即消失。
3. conntrack 仍存在。
4. 软件 forwarding 继续。
5. 重新加载 ruleset。
6. 旧连接不错误复用旧 flowtable，新连接可重新进入硬件。

## 11. G6：硬件统计、老化、复用、容量和碰撞门

### 11.1 SDT29 和 lastused

|ID|项目|PASS 条件|
|---|---|---|
|G6.1|zero traffic read|插入后立即读|packet/byte 仍 0；插入本身不刷新 lastused|
|G6.2|exact counter|发送已知 N 包/字节|SDT29 delta 与硬件字节口径一致|
|G6.3|hardware-driven lastused|只有硬件 counter 增长才刷新|空读不刷新|
|G6.4|active aging|5/30/60 s 有反向流量|持续 `[HW_OFFLOAD]`|
|G6.5|idle aging|停止流量|约 28 s 仍在、约 34 s 离开硬件；conntrack 仍可存在|

### 11.2 复用和普通容量

直接使用：

- `flow-stats-reuse.sh`：100 add/zero-stats/delete/reuse cycles。
- `flow-stats-capacity.sh`：256 concurrent 基础统计，再做 2,048 bidirectional NAT connections。
- `flow-zcam-collision.sh`：32 bidirectional collision pairs。

PASS：

- 100 轮无 ID 泄漏、重复 cookie 错误、旧 counter 污染。
- 256 connections/512 directions 全部进入硬件。
- 2,048 connections/4,096 SDT43 directions 成功。
- 第 2,049 个连接原子 `-ENOSPC`，不留下半条双向 rule。
- 删除后 hardware count、IKEY、age、ZCAM、counter ID 回到 baseline。
- 32 collision pairs/64 directions 都通过剩余 cell/block 候选进入硬件。

### 11.3 SDT14 DDR

必须覆盖：

1. 第 257 个 full SDT14 flow 从 ZCAM overflow 到 DDR。
2. 指定 key 命中预期 bucket。
3. 外部 comparator 随流量增加。
4. 删除后 valid bit 和 bucket body 清理。
5. 重建后同一 key 可复用相同 bucket。
6. deliberate same-bucket collision：第一条 DDR，第二条 ZCAM stash，二者都转发。
7. 高于 4096 的 age index read-clear。

容量门：4,300 bidirectional PPPoE connections / 8,600 full SDT14 directions；预期首 256 directions 在 ZCAM，其余进入 DDR 或 ZCAM collision stash。所有连接必须 `[HW_OFFLOAD]`，删除后 hardware connection count 为 0。

注意：262,144 buckets 是设计容量，不是本轮必须达到的实测连接数；不得把它写成已验证上限。

## 12. G7：IPv6 routed offload 门

使用 [nft-ipv6-flowtable.nft](/Volumes/code/zx279133/port/mainline/nat-acceptance/nft-ipv6-flowtable.nft)。`meta nfproto ipv6`、established、L4 predicate、`flow add @fast` 必须在同一条 nft statement。

|ID|项目|PASS 条件|
|---|---|---|
|G7.1|IPv6 TCP LAN→WAN|2 GiB payload|`[HW_OFFLOAD]`；≥ 2.25 Gbit/s；payload 完整|
|G7.2|IPv6 TCP WAN→LAN|2 GiB payload|`[HW_OFFLOAD]`；≥ 2.15 Gbit/s；payload 完整|
|G7.3|IPv6 UDP 双向|双方向硬件规则和 endpoint counters|无丢包/错误 tuple|
|G7.4|CPU/softirq|1–2 GiB 单流|busy ≤ 1%，无持续 softirq 增长|
|G7.5|ICMPv6 fallback|ping6|工作但没有 HW marker|
|G7.6|NAT66 negative|尝试 unsupported translation|正确软件处理/拒绝，不能错误 `in_hw`|

## 13. G8：PPPoE IPv4/IPv6 门

### 13.1 PPPoE 基础

- pppd 建链、SID、MTU/MRU 正确。
- WAN raw capture 只看到 `0x8864` session traffic。
- IPv4 PPP protocol 正确。
- link down/up、pppd stop/start 可重复 20 轮。

### 13.2 IPv4 over PPPoE

|ID|项目|PASS 条件|
|---|---|---|
|G8.10|TCP/UDP 双向|双方 established|push/pop 两方向 `[HW_OFFLOAD]`|
|G8.11|wire capture|WAN pcap|SID 正确，无错误 bare IPv4 leak|
|G8.12|peer→Windows TCP|单流|≥ 2.20 Gbit/s，无 retransmission|
|G8.13|Windows→peer TCP|单流|≥ 0.85 Gbit/s；标明 VM RX ceiling，不称路由器上限|
|G8.14|flush/rebuild|活动连接中 flush|软件 PPPoE 继续；新连接可再次 offload|

### 13.3 IPv6 over PPPoE

|ID|项目|PASS 条件|
|---|---|---|
|G8.20|/128 route|双方 peer route|ping6/TCP/UDP 正确|
|G8.21|TCP/UDP 双向|活动连接|两方向 `[HW_OFFLOAD]`|
|G8.22|wire protocol|WAN pcap|EtherType `0x8864`、SID 1、PPP `0x0057`；无裸 `0x86dd`|
|G8.23|lifecycle|flush、pppd restart、link flap|WANID0/16、SID、ZCAM/DDR、counter/age 清理和复用|

当前源码权威行为是 PPPoE push/pop 都使用 full SDT14；旧文档中 “compact SDT43 pop” 是历史残留，验收结果不得沿用旧预期。

## 14. G9：checksum、EEE、XDP/AF_XDP、PHC 门

### 14.1 TX checksum

当前硬件 TX checksum 只支持非封装 IPv4 TCP。

|ID|项目|预期|
|---|---|---|
|G9.1|IPv4 TCP CHECKSUM_PARTIAL|`tx_hw_csum_packets` 增长，无 wire bad checksum|
|G9.2|IPv4 UDP|软件 checksum fallback；`tx_sw_csum_packets` 增长|
|G9.3|IPv6 TCP/UDP|软件 fallback；wire checksum 正确|
|G9.4|encapsulated/unsupported|软件 fallback，不误开硬件 bit|
|G9.5|RX checksum|当前没有 RXCSUM|协议栈软件校验正确；不得声称硬件 RX checksum|

### 14.2 EEE

1. `ethtool --show-eee eth0` baseline。
2. EEE off：link 和 2.5G throughput baseline。
3. EEE on，TX LPI 1000，advertise 已验证的 1G/2.5G modes。
4. 等待重新协商，要求 `enabled - active`。
5. 重跑 30 s IPv4 NAT throughput，下降不得超过 5%，无 link flap。
6. link down/up、driver stop/start 后配置和行为符合预期。
7. 恢复验收定义的最终 EEE 状态。

### 14.3 AF_XDP zero-copy

现有 `/bin/xsk-zc` 可验证：driver-mode XDP attach、XSKMAP redirect、UMEM fill/RX/TX/completion、need-wakeup、zero-copy flag、XSK statistics。

PASS 条件：

- 输出 `AF_XDP zero-copy active`。
- `XDP_OPTIONS_ZEROCOPY` 成立。
- 60 s 内 `rx > 0`、`tx > 0`、`completions > 0`。
- driver `xsk_*`、`xdp_redirect`/`xdp_tx` counter 与测试相符。
- 结束后 BPF link、XSK pool、UMEM、RX pages 全部释放。
- 100 次 attach/run/detach 无 page/DMA/ring 泄漏。
- 单独覆盖 link down/up 和 MTU 变化的正确失败/恢复。
- 非零 queue ID 正确返回 `-EINVAL`，不崩溃。

### 14.4 plain XDP

当前仓库没有独立的 PASS/DROP/TX/REDIRECT smoke harness。`xsk-zc` 只充分覆盖 redirect-to-XSK 和 XSK TX。

若不增加一个测试用 `xdp-smoke` 或 staging `bpftool/xdp-loader`，以下能力必须标 BLOCKED：

- XDP_PASS 数据完整性
- XDP_DROP 精确 drop counter
- XDP_TX L2 echo
- 普通 XDP_REDIRECT 到另一 netdev
- invalid action → aborted/exception

增加测试程序不属于增加驱动功能，但必须进入 artifact manifest。

### 14.5 PHC/PTP

使用 `/bin/ptp-test`：

```sh
ptp-test clock /dev/ptp0 60
ptp-test set /dev/ptp0 SEC NSEC
ptp-test adjfreq /dev/ptp0 1000
ptp-test tx eth0 192.168.1.100
ptp-test rx eth0
```

PASS/预期：

- 60 个 PHC sample 单调递增，约每秒增长 1 秒。
- settime 后读取为设置值。
- link down 时读时钟按当前设计返回 `ENETDOWN`；link up 后 PHC 恢复前进。
- `adjfreq` 正确失败，因为 `max_adj=0`。
- TX/RX hardware timestamp 配置正确失败，因为当前未实现 packet hwtstamp。
- ethtool 只能宣称基础 PHC 和 software timestamp，不能宣称完整 PTP。

## 15. G10：故障恢复和资源生命周期门

|ID|故障/操作|重复|PASS 条件|
|---|---|---:|---|
|G10.1|nft flush/reload|100|每轮硬件归零，新连接可重建|
|G10.2|TCP FIN|100|flow/conntrack 按期清理|
|G10.3|TCP RST|100|立即/按期清理，无 stale entry|
|G10.4|UDP idle timeout|100|hardware 先老化，conntrack 后老化|
|G10.5|eth0 link down/up|20|flow 删除/失效，link 恢复，新流可 offload|
|G10.6|lan port link down/up|每口 20|DSA/MIB/FDB/flow 状态正确|
|G10.7|pppd stop/start|20|SID/WANID/SDT14 清理和复用|
|G10.8|DSA module unload/reload|20，仅无活动业务时|netdev 和 parent datapath 生命周期恢复|
|G10.9|direct `reboot -f` + `bootm`|10|每轮基础 gate 和 smoke NAT 通过|
|G10.10|watchdog reset|1|预期复位并重新进入可验收状态|

任何 unload/reload 前必须清空 nft/tc/bridge/PPPoE，不能用活动引用强行卸载。

## 16. G11：混合并发和长稳门

### 16.1 非 PPPoE 混合并发

至少同时运行：

- 16 个 IPv4 TCP NAT flows
- 16 个 IPv4 UDP NAT flows
- 8 个 IPv6 TCP routed flows
- 8 个 IPv6 UDP routed flows
- ICMP 和 ICMPv6 software fallback probes
- VLAN-aware bridge/PVID 100
- 周期性反向小包，防止 UDP 单向 timeout

持续 2 小时，每 60 秒采集：

- conntrack HW marker count
- hardware flow count
- SDT29/age samples
- endpoint bytes/packets
- IRQ/softirq/CPU
- ethtool/MIB errors/drops
- dmesg delta

### 16.2 PPPoE 混合并发

同一个 SID 下同时运行：

- IPv4 TCP/UDP 双向
- IPv6 TCP/UDP 双向
- pcap 持续抽样
- stats/age 抽样

持续 2 小时。不能启动第二 SID，因为那不是当前支持能力。

### 16.3 8 小时 soak

选择前两组中最贴近产品的组合持续 8 小时：

- 不丢 HW marker，除非符合规定的 idle/FIN/RST。
- 不出现持续 resource 单向增长。
- RX page map、TX pending、counter ID、ZCAM/DDR used 应在稳定区间。
- endpoint packet difference 不持续扩大到门槛以上。
- 无 link flap、watchdog reset、kernel health error。
- 每小时创建和销毁一批新连接，证明老流存在时资源仍可循环。

AF_XDP 会接管同一 netdev 的报文，不应与路由 soak 在同一时段运行；它有独立 100-cycle gate。

## 17. Cleanup 和最终资源基线

每个 major gate 结束必须：

```sh
nft flush ruleset
tc qdisc del dev eth0 clsact 2>/dev/null || true
tc qdisc del dev lan1 clsact 2>/dev/null || true
ip link set ppp0 down 2>/dev/null || true
```

并确认：

- `/proc/net/nf_conntrack` 无本 gate 连接。
- hardware connection count 为 0。
- TC filter 无 `in_hw` rule。
- 目标 ZCAM/DDR buckets invalid/clear。
- counter IDs 和 age resources 回到 baseline。
- WANID0 source IP 恢复。
- WANID0/16 PPPoE mode/SID 恢复。
- BPF/XDP link 和 XSK pool 不存在。
- bridge/VLAN/FDB 恢复预定 baseline。
- link 和 host IP 恢复。
- Mac 被临时移除的地址恢复。
- serial、tcpdump、iperf、QEMU 额外进程明确保留或关闭。

最终执行一次 direct boot smoke，确认验收本身没有留下跨重启依赖。

## 18. 现有夹具覆盖与缺口

### 18.1 可直接复用

|能力|现有夹具|
|---|---|
|IPv4 nft auto NAT|`nft-flowtable.nft`|
|IPv6 route offload|`nft-ipv6-flowtable.nft`|
|PPPoE flowtable|`nft-pppoe-flowtable.nft`|
|UDP endpoint|`udp-echo.c`、`udp-pacer.c`、`UdpPaced.cs`|
|TCP endpoint|`TcpStream.cs`|
|TC flow helper|`tc-udp-flow.sh`|
|stats reuse|`flow-stats-reuse.sh`|
|SDT43 capacity|`flow-stats-capacity.sh`|
|ZCAM collision|`flow-zcam-collision.sh`|
|SE/SDT29 read|`se-stat-read.c`|
|AF_XDP|`xsk-zc.c`|
|PHC/PTP negative/positive|`ptp-test.c`|
|watchdog baseline|initramfs `init`|

### 18.2 必须补测试夹具，才能完成“大验收”

1. 总编排器：逐 gate 运行、超时、采集、cleanup、最终 TSV。
2. host/board 状态采集器。
3. plain XDP PASS/DROP/TX/REDIRECT smoke tool。
4. 四 LAN 口人工换线提示和逐口结果模板。
5. EEE 自动 A/B 采集。
6. PVT、LED、key、SFC read-only、USB read-only 小脚本。
7. TCP FIN/RST、UDP idle、link flap、pppd restart 循环器。
8. 混合 IPv4/IPv6 flowtable ruleset；当前三个 nft 文件会各自 `flush ruleset`，不能直接并发复用。
9. 统一 dmesg fatal-pattern scanner。
10. 资源 baseline/delta 检查器。

这些都是验收基础设施，不改变驱动能力。但在它们完成前，对相应用例必须保留 BLOCKED。

## 19. 推荐执行日程

|阶段|预计时间|
|---|---:|
|G0 构建/manifest|1 小时|
|G1/G2 启动和板级|1.5 小时|
|G3 四 LAN/DSA|2 小时|
|G4/G5 慢路径和 IPv4 NAT|1.5 小时|
|G6 stats/capacity/collision/DDR|3–4 小时|
|G7 IPv6|1 小时|
|G8 PPPoE IPv4/IPv6|2 小时|
|G9 checksum/EEE/XDP/PHC|2 小时|
|G10 fault/reboot|2–3 小时|
|G11 soak|至少 8 小时|

完整 release qualification 预计需要 1.5–2 天，不应压缩成一次几个小时的 smoke test。

## 20. 最终判定

### Release PASS

必须同时满足：

- G0–G8 所有非 fixture-conditional 项 PASS。
- G9 中已有实现能力全部 PASS 或按设计正确 negative PASS。
- G10 无资源/恢复失败。
- G11 两组混合并发和 8 小时 soak PASS。
- 四个 LAN 口全部 PASS。
- 最终 cleanup 和 direct boot smoke PASS。
- 全程无 kernel health fatal pattern。

### Release BLOCKED

任一当前已实现能力因为缺 fixture 无法测试，例如 plain XDP 或 USB，则最终只能写 BLOCKED/PASS WITH UNVERIFIED ITEMS，不能写“当前能力全部验收通过”。

### Release FAIL

功能正确但 cleanup、重复性、容量、link recovery、CPU/softirq 或 kernel health 失败，也必须整体 FAIL；大验收不是只看一次转发成功。

## 21. 最终报告最少内容

最终报告必须包含：

1. artifact manifest 和 hash。
2. 拓扑和物理接线。
3. 每个 case 的 PASS/FAIL/BLOCKED 表。
4. 性能原始端点计数，不只写 Gbit/s。
5. offload 四证据。
6. 资源 before/after。
7. reboot/link/flush/restart 次数和失败索引。
8. 8 小时 soak 时间序列。
9. 所有 dmesg fatal-pattern 结果。
10. 明确的未验证项；不允许用“其余同理”。

## 22. 证据入口

- [当前能力审计](/Volumes/code/zx279133/docs/SR1010_KERNEL_CAPABILITY_AUDIT_2026-08-31.md)
- [NPPT forwarding 验收](/Volumes/code/zx279133/docs/mainline-network/NPPT_FORWARDING.md)
- [FAST stats 研究与验收](/Volumes/code/zx279133/docs/mainline-network/FAST_STATS_RESEARCH.md)
- [IDM descriptor](/Volumes/code/zx279133/docs/mainline-network/IDM_DESCRIPTOR.md)
- [NAT/IPv6/PPPoE 验收说明](/Volumes/code/zx279133/port/mainline/nat-acceptance/README.md)
- [诊断 init](/Volumes/code/zx279133/port/mainline/initramfs/init)
- [FIT builder](/Volumes/code/zx279133/port/mainline/build-zxdbg.sh)
- [AF_XDP fixture](/Volumes/code/zx279133/port/mainline/xdp-acceptance/xsk-zc.c)
- [PTP fixture](/Volumes/code/zx279133/port/mainline/ptp-acceptance/ptp-test.c)
