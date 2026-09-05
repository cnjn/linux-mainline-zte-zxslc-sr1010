# SR1010 当前主线：全部已驱动硬件验收清单

日期：2026-09-03

这份清单取代“先做一整套发布认证”的思路。目标只有两个：

1. 当前主线已经驱动的每个硬件块，都执行一次真实功能操作；不能只看 probe 成功。
2. 对外宣称“好用”的功能，再做三次重复、短时负载和错误日志检查。

当前编译 DTB 中有 49 个 active compatible node。固定时钟、syscon、reset provider 等基础节点合并到实际 consumer 中验，不逐寄存器制造测试项目。

## 1. 统一判定

每项只记一行：`PASS / FAIL / BLOCKED / NOT SUPPORTED`。

- `PASS`：真实硬件动作成功，重复三次，期间没有新增内核错误。
- `FAIL`：功能错误、数据错误、卡死、资源不释放或出现内核错误。
- `BLOCKED`：缺 USB 盘、可调速网卡、四根网线等外部条件，不能用“probe 成功”替代。
- `NOT SUPPORTED`：当前驱动明确没有该能力，同时确认没有错误广告给用户空间。

每项最少证据：执行命令、实际输出或物理现象、测试前后 `dmesg` 差异。网络硬件加速另外保留端点包数、硬件计数和 CPU/softirq。

全部测试只改 RAM 状态；禁止 `saveenv`、flash erase/write 和持久化网络配置。eFuse 只保存长度和哈希，不保存原始内容。

## 2. 平台核心

|硬件块|必须实际验证的功能|通过条件|
|---|---|---|
|双 Cortex-A53、PSCI|两核 online；分别跑 CPU 负载；`reboot -f`|两核都执行任务，无 stall/SError；重启成功|
|GICv3、ARM timer|观察 timer、网卡、GPIO、USB IRQ 增长；运行高精度定时测试|IRQ 落到预期 CPU，计时误差稳定，无中断风暴|
|TOPCRM/LSP0/LSP1 clocks、reset、syscon|通过 UART、SFC、WDT、MDIO、USB、网络 consumer 的 probe 和真实 I/O 联合验证|所有 consumer 工作；无 clock/reset timeout|
|PL011 UART|完整启动日志；串口输入、输出和连续传输|无乱码、丢字符、卡死；console 可持续操作|
|DDR/内存路径|占用大部分可用内存进行写入、校验、释放|数据校验正确，无 OOM、ECC/SError、内存破坏|

## 3. 板级控制器和外设

|硬件块|必须实际验证的功能|通过条件|
|---|---|---|
|5 个 GPIO bank + irqchip + pinctrl|读取 gpiochip；用 LED 输出和 reset key IRQ 覆盖实际引脚；网络 reset GPIO 由 PHY/switch 冷启动覆盖|输入、输出、IRQ 均有真实动作；无重复/丢失 IRQ|
|白 LED、红 LED|分别 on/off，切换 timer/heartbeat trigger|肉眼状态与命令一致，三轮切换正常|
|reset key|读取 input event，短按三次|每次恰有 press/release；不触发恢复出厂|
|PVT hwmon/thermal|连续读取 `temp1_input`、`in1_input`；CPU 负载前后比较；核对 thermal zone|数值范围合理、连续、负载后温度趋势正确，无读数卡死|
|eFuse NVMEM + ttrim|读取 0x80-byte shadow 两次并计算哈希；确认 PVT ttrim cell 可消费|长度正确，两次哈希一致，PVT 正常；不记录原始 eFuse|
|SFC + SPI-NAND + MTD partitions|识别 flash；从每个只读分区抽样读取两次并比对哈希；观察 ECC 统计|分区布局正确，重复读取一致，无新增不可纠正 ECC；绝不 erase/write|
|watchdog0|保持 watchdog1 feeder，停止 watchdog0 feeder，记录超时和复位|在配置超时内发生整机复位，复位后能 RAM 启动|
|watchdog1|保持 watchdog0 feeder，停止 watchdog1 feeder，记录超时和复位|在配置超时内发生整机复位，复位后能 RAM 启动|
|USB xHCI USB2/USB3|热插拔；USB2 和 SuperSpeed 分别枚举；在专用测试盘上创建、校验、删除测试文件|速率模式正确，文件哈希一致，三轮热插拔无错误|

watchdog 实际复位放在最后执行。USB 只写专用测试文件，不做 raw-device 覆盖。

## 4. PHY、SerDes、MAC 和 DMA

|硬件块|必须实际验证的功能|通过条件|
|---|---|---|
|MDIO0 + RTL8372N|读 chip/PHY ID、链路状态和 MIB；链路 down/up|读值稳定，状态与物理链路一致|
|MDIO1 + ZX279051|读 PHY ID；读取/设置标准 phylink 能力；链路 down/up|PHY 可访问，协商和恢复正常|
|PON SerDes、Uni SerDes、XPCS0/1|冷启动 lock；链路反复 down/up；1G/2.5G 切换|每轮重新 lock，MAC/PCS/PHY 状态一致|
|IDM RX/TX rings、32-bit DMA、page-pool、NAPI|双向小包和大包；持续流量；查看 ring、DMA、NAPI 和错误统计|无 DMA 错误、ring stall、异常丢包或地址越界|
|TX completion、BQL、TX watchdog recovery|持续发送并执行一次可控 link flap|队列正常停止和唤醒，无 NETDEV WATCHDOG；完成计数闭合|
|WAN MAC|64、1500、1970 MTU 边界；TCP/UDP 双向；1G/2.5G|边界行为正确，双向稳定，错误计数不增长|
|LAN MAC/conduit|lan1..lan4 分别做链路、ping、TCP/UDP 和 MTU 测试|四口结果等价，没有只能工作的单一 LAN 口|
|EEE|1G、2.5G 下 enable/disable A/B，观察 active 状态、吞吐和错误计数|协商状态正确；开启后不掉速、不丢包、不掉链|
|TX checksum|IPv4 TCP 硬件 checksum；UDP、IPv6、封装流量验证软件 fallback|抓包校验正确；只广告和执行已实现范围|

RX checksum、SG、TSO/GSO/LRO、RSS/多队列当前没有实现，不做伪功能验收；只检查 netdev 没有错误广告这些 feature。

## 5. RTL8372N DSA 交换机

|已实现功能|验法|通过条件|
|---|---|---|
|四个 user port|lan1..lan4 逐口收发和 MIB 对账|四口均能独立收发，端点与 MIB 基本闭合|
|内部 PHY/phylink|标准 ethtool 读取能力；限制 advertisement 到 1G；恢复全能力|四个 PHY 绑定；10/100/1000/2500 能力正确；真实协商 2.5G→1G→2.5G|
|EEE/线缆诊断|标准 ethtool EEE on/off；在线和断链 `--cable-test`|EEE 触发重协商且恢复；在线四对线 OK/5m；断链 RTCT 完成并上报状态|
|bridge join/leave|四口加入/退出 bridge；检查私有 S-VLAN transport 和硬件转发标记|状态切换立即生效，无残留转发|
|VLAN filtering/add/del|PVID access VLAN、tagged trunk、VID 0 priority tag；至少两个 VLAN 隔离|同 VLAN 可通、跨 VLAN 隔离；删除后状态清理|
|STP state|逐口切换 disabled/blocking/forwarding|实际转发行为与 STP 状态一致|
|MSTP|VLAN100 绑定 MSTI1；lan1 在 blocking/forwarding 间切换|标准 bridge MST 状态可往返；blocking 断流，forwarding 立即恢复|
|bridge flags|逐项切换 learning、unicast/multicast/broadcast flood 和 isolated|Linux 状态、寄存器和真实转发行为一致；恢复后学习和转发立即恢复|
|ageing/fast-age|修改 bridge ageing time；端口阻塞触发动态 FDB flush|寄存器读回一致，动态项删除而静态项保留|
|FDB add/del/dump|动态学习、静态添加、删除和 dump|MAC/port 映射正确，删除后不再命中|
|MDB/IGMP/MLD|静态 IPv4/IPv6、SVL/IVL；真实 IGMP/MLD join/leave|动态项显示 `temp proto kernel offload`；socket close 后自动删除；硬件成员掩码与 Linux MDB 一致|
|LAG|`balance-xor` 的 layer2、layer2+3、layer3+4 hash；成员增删、active mask、静态 FDB、链路切换和完整拆除|组成员/hash/active 寄存器与 Linux 状态一致；静态 FDB 随 active member 迁移；down/up、leave/rejoin、删除复用和 teardown 无残留|
|port mirror|tc matchall ingress/egress mirror；冲突 monitor 负测|规则 `in_hw`，硬件 matched/sample 计数增长|
|port policer|tc matchall ingress police；100 Mbit/s、2 MiB burst A/B|规则 `in_hw`；受限约 96 Mbit/s，删除后恢复约 499 Mbit/s|
|ACL/tc flower|L2 EtherType/MAC、IPv4、完整 IPv6、C/S/QinQ VLAN、L4 port/range；pass/drop/mirror/redirect/trap、`skbedit priority`、IPv4/IPv6 DSCP pedit、C-VLAN push/pop/modify；replace、排序、删除复用|规则 `in_hw`；IPv4/IPv6/range 真实数据面按规则变化；trap 保留来源 SVID 并到达 CPU；push 抓到 SVID62+CVID100/PCP3；DSCP 从 `0x00` 改为 `0xb8`；VID 100→200 modify 精确命中 3 包；删除/替换后无旧动作残留|
|ACL shared meter|flower `action police` 100/200 Mbit/s；replace、delete、reuse|真实接收 99.8/198 Mbit/s；删除恢复 498 Mbit/s；同 meter 可复用|
|ACL logging stats|固定包数、空闲二读、删除复用、四模板 IPv6|`tc -s` packet/`lastused` 精确；5 包保持为 5，复用后从 0 计 2 包，IPv6 计 3 包|
|DCB priority|`dcb app` default/DSCP map；`dcb apptrust` PCP/DSCP|配置可往返；第三种并发 policy 按两 profile 硬件上限返回 `ENOSPC`|
|ETS/TBF QoS|8-band strict/WFQ、priority→queue、root/queue TBF|删软件 TBF 后 queue0 仍为 96.2 Mbit/s；映射到 queue1 恢复 197 Mbit/s；生产删除闭合|
|per-port MTU|max MTU 和越界值|合法值生效，越界值明确拒绝|
|ethtool MIB/pause/register dump|已知包数双向发送；读 pause-frame；抓 HSB parser snapshot；读 ACL logging counter block|对应端口计数增长；`ethtool -d` 可读 template/selector、counter 和指定 ingress port HSB，无异常错误|
|offline self-test|芯片/PHY 寄存器访问；64/1518 字节 MAC local-loopback；测试前后真实 LAN 连通|四项均 PASS；TX/RX broadcast MIB 同步增长；退出后恢复 loopback 位并 fast-age 动态 FDB，Windows ping 不中断|

devlink 当前提供 ASIC ID，resource/health 尚未实现。ACL 已接通硬件
template 0..4、16 个 parser
selector、完整 IPv6、C/S/QinQ VLAN、16 项 L4 range table、per-rule shared
meter、CPU trap，以及 32 项 packet logging counter。logging 与 policing 共用
action selector，因此 police 规则不能同时提供 logging counter；普通规则可用
`hw_stats disabled` 省下 counter。CPU8 采用原厂式 S-VLAN transport，端口
4..7 分别使用私有 SVID59..62，客户 C-VLAN 因此可独立 push/pop/modify。
push 和 modify 已完成真实数据面 A/B；pop 已有硬件 packet-hit，仍需带
VLAN100 CPU membership 的 tagged fixture 完成端点 A/B。QoS 已接通标准
DCB、ETS 和 root/queue TBF；ACL DSCP remark 已实现，CBS/TAPRIO/ETF 和
per-queue stats 尚未实现。
RTL 自主 IGMP/MLD 私有组表刻意不开启，由 Linux bridge snooping 统一
管理动态 MDB，避免双控制面竞态。LAG 的完整控制面生命周期、单 active
member 转发和链路切换已经验收；双在线成员的按 hash 分流，以及 MDB 的
直接 LAN-to-LAN 复制仍需第二个在线 LAN 端点做最终数据面验收，不能用
单端点的控制面成功替代。

## 6. XDP、AF_XDP 和 PHC

|功能|验法|通过条件|
|---|---|---|
|XDP PASS|挂载 PASS 程序跑流量|普通协议栈收包正确，XDP 计数正确|
|XDP DROP|按条件丢包|端点收不到，DROP 计数精确增长|
|XDP TX|收到后原口返回|回包和 TX 计数正确，无 buffer 泄漏|
|XDP REDIRECT/ndo_xdp_xmit|WAN/LAN 间 redirect|目标口收到，源/目标统计闭合|
|XDP ABORTED|受控触发少量异常 action|trace/统计可见，驱动不崩溃|
|AF_XDP zero-copy queue 0|fill、RX、TX、completion、need-wakeup 全生命周期|零拷贝实际收发，ring 可持续复用，退出后资源释放|
|AF_XDP/XDP 限制|queue 1、fragments/multi-buffer 负测|分别返回预期错误，不静默接受|
|PHC|`phc_index`、get、set；link down 读失败；link up 后继续走时|时间可读写，down/up 生命周期符合实现|
|PHC 限制|adjfreq、hwtstamp ioctl、PPS/extts 负测|明确拒绝且不错误广告完整硬件时间戳|
|RTL8372N PTP capability probe|内部源配置/当前频率、TOD 走时；外部源 apply|版本 `0x201` 接受 `0x10000000` 配置但 TOD 不走，外部源超时；驱动必须不注册死 PHC，保留 SoC `zx279133-tod`|

## 7. NPPT 硬件流表

|硬件路径|最小有效验法|通过条件|
|---|---|---|
|IPv4 route|TCP、UDP 各一条双向流|端点正确；硬件命中；CPU/softirq 低|
|IPv4 NAT|SNAT、DNAT、TCP/UDP port rewrite|地址、端口、checksum 正确；双向硬件命中|
|IPv6 route|TCP、UDP 各一条双向流|端点正确，SDT/IKEY 命中，无软件瓶颈|
|PPPoE IPv4|拨号、TCP/UDP 双向、断开重拨|线上是 0x8864/SID；push/pop 正确；重拨后恢复|
|PPPoE IPv6|TCP/UDP 双向|PPP 0x0057 正确，无裸 0x86dd 泄漏|
|flower frontend|`skip_sw` route/NAT 规则|`in_hw` 且实际硬件计数增长|
|nft flowtable frontend|自动学习连接|conntrack `[HW_OFFLOAD]` 且实际硬件命中|
|SDT29 stats/lastused|固定包数、空闲、再次发包|packet/byte 精确；只有真实流量刷新 lastused|
|age/lifecycle|建流、空闲老化、删流、同 ID 复用|到期删除，无旧计数和旧转发残留|
|ZCAM collision fallback|使用现有碰撞夹具制造一组冲突|冲突流仍正确转发，删除后 cell 释放|
|SDT14 DDR overflow|制造少量 ZCAM 之外的 DDR entry，不跑极限容量|DDR comparator 实际命中，清理后 bucket 可复用|
|flush/fallback/rebind|流量中 flush，再重建 flowtable|硬件标志立即消失，软件不断流，新流可再次卸载|
|短时性能|每种主要路径跑 60 秒双向流|接近已验证吞吐，CPU/softirq 和丢包无明显回退|

不需要每次都重跑 4,300 连接极限。容量边界是专项回归；本轮只证明 ZCAM、DDR、统计和生命周期各自真的工作。

## 8. 不在“已驱动硬件 PASS”里的项目

当前镜像没有可用生产路径：I2C、PWM、PCIe、RapidIO、WLAN、cpufreq/DVFS、suspend、硬件 crypto。

当前网络驱动仍没有完整通用 NIC/交换机能力：RSS/多队列、WoL、DSA
devlink resource/health、CBS/TAPRIO/ETF 和 per-queue stats。RTL8372N 已有
专用 offline self-test；内部 PHY 已接入标准 phylib，支持真实链路协商、EEE 和
ethtool cable-test；bridge MSTP 已接入 VLAN→MSTI、端口状态和 VLAN
fast-age。IGMP/MLD 已走 Linux bridge snooping 到硬件 MDB；RTL8372N ACL
template 0..4、CPU trap、DSCP/C-VLAN action 和 logging counter 已接到标准
flower/tc stats。C-VLAN pop 的 tagged endpoint A/B 仍待补齐，但不再受 CPU
transport tag 架构阻挡。

这些项目在最终报告中列为 `NOT SUPPORTED`，不能因为没有报错而写成 PASS。

## 9. 最短执行顺序和时间

1. 平台核心、UART、PVT、eFuse、SFC 只读：约 30 分钟。
2. LED、按键、GPIO、USB：约 45～60 分钟。
3. PHY/SerDes/MAC、四个 LAN 口、DSA：约 60～90 分钟。
4. XDP、AF_XDP、PHC、NPPT 全路径：约 90～150 分钟。
5. watchdog0/1 真实复位：约 30 分钟，最后执行。

外部设备齐全时，一次完整功能验收约 4～6 小时。最终只输出一张结果表和失败项证据，不输出几百行过程日志。
