# SR1010 五网口 LED

日期：2026-09-13。

## 2026-10-20 端口编号更正

用户确认此前 lan1..lan4 与机壳物理编号相反。当前设备树已改为交换芯片
端口 4/5/6/7 对应 lan1/lan2/lan3/lan4；LED 编号和验收脚本随同反转，
保持每个插座的交换端口与 GPIO 配对不变。
当前映射见 [PORT_LED_IMPLEMENTATION.md](PORT_LED_IMPLEMENTATION.md)。

下文保留 2026-09-13 的历史验收记录，其中 LAN 名称均为当时的旧编号。
此次仅完成源码、设备树编译及映射读回检查，尚未在新镜像上重新逐口插拔验收。
升级现有配置时，旧 lan1/lan2/lan3/lan4 分别改为新 lan4/lan3/lan2/lan1；
例如旧 lan4 上的 WAN2，应改用新 lan1，以保留原接线。

## 实现与根因

后面五个插座的指示灯由 **SoC GPIO** 驱动。它们没有接到此前测试的
RTL8372N 自动 LED 输出。主线设备树此前仅声明了机身红、白状态灯，
既没有这五个 GPIO LED，也没有绑定网口链路状态。

原厂 `zte_gpio_init()` 选择 `zte_boardparam[23]`，该板 GPIO 表的
前五项分别是 2、13、68、69、26，均设为输出，初值 1。
通过原厂 Telnet 的逐灯置高/置低测试，用户分别确认了下列对应关系。
WAN、LAN1 测试时链路保持连接，排除了误操作 PHY reset 的解释。

| 插座 | SoC GPIO | 设备树 GPIO | netdev | 有效电平 |
| --- | --- | --- | --- | --- |
| WAN | 26 | gpio1[10] | eth0 | 低 |
| LAN1 | 69 | gpio4[5] | lan1 | 低 |
| LAN2 | 68 | gpio4[4] | lan2 | 低 |
| LAN3 | 13 | gpio0[13] | lan3 | 低 |
| LAN4 | 2 | gpio0[2] | lan4 | 低 |

实现使用标准 `gpio-leds` 和 `netdev` LED trigger：设备树声明引脚、
低有效和默认灭灯，启动脚本将每盏灯绑定到对应 netdev 并启用 `link`。
同时自动加载 LAN DSA 驱动、启用 conduit 和四个 LAN 口。

默认行为为连接亮灯、断开灭灯；`rx=0`、`tx=0`。
LAN 的 `get_stats64()` 需要通过 MDIO 读取交换机 MIB，默认只指示链路
可以避免为闪灯周期性读取统计。标准触发器的收发闪烁选项仍可配置，
没有增加自定义定时器、PHY LED 猜测字段或数据路径钩子。

## 验证

- 五根 GPIO 的插座映射及低有效均经原厂实物确认。
- `JOBS=8 ./port/mainline/build-zxdbg.sh` 成功；启动脚本语法检查通过。
- 主线设备树补丁 checkpatch 严格检查无错误、警告或检查项。
- 新镜像已通过 TFTP RAM 启动；五个 LED 均存在并自动绑定。
- `verify-port-leds.py` 五项全部通过：carrier、brightness、GPIO 方向、
  物理电平及 link/rx/tx 设置一致。初始接线下 WAN、LAN1 为亮，
  LAN2、LAN3、LAN4 为灭。
- 提速版 WAN ping `192.168.1.100` 为 3/3，Windows 经 LAN1 链路本地
  IPv6 ping 为 3/3；IPv6 RSS 参数保持启用。内核 taint 为 0，仅加载
  生产 DSA 模块，没有留下诊断模块。
- 用户已逐一插拔 WAN、LAN1、LAN2、LAN3、LAN4，确认五个口均只亮
  对应的灯，插线亮、拔线灭。

## 响应速度

用户反馈亮灭反应偏慢后，核对发现这两类 PHY 使用 PHYLIB 默认的
`PHY_STATE_TIME = HZ`，约每秒检查一次。LED 的 link 模式通过 netdev
通知更新，没有另一个用于亮灭的定时轮询。

ZX279051 和 RTL8372N 内部 PHY 现在通过现有 `get_next_update_time`
回调使用 250 ms 周期。这个值是软件轮询间隔，不是从插线到亮灯的
总时延保证；2.5G 自动协商及调度仍需要时间。

提速版已重新 RAM 启动，五灯状态再次全部通过。通过现有 MDIO sysfs
统计，以约 50 ms 间隔采集 50 个样本，WAN 管理读取批次间隔约
0.25～0.31 秒；LAN1 除一次跨采样边界的批次拆分外也在该范围。
这是粗粒度管理读取观测，不是精确的插线到亮灯时延测量。

原厂也没有在这条已恢复的路径中使用链路中断：
`smac_check_phy_task_thread()` 检查各 MAC 的 PHY 后调用
`msleep_interruptible(100)`。当前主线没有已验证的 PHY 链路 IRQ
接线、使能和清除处理，不能拿 IDM 收发包中断代替外部 PHY 链路中断。
证据见 `docs/reverse-engineering/plat_132/functions/0x12890-smac_check_phy_task_thread.md`。

最终提速版 FIT：`out/sr1010-zxdbg.itb`，11012980 字节，SHA-256：

```
10771cf7f2359ed529b29e2c94a811606b86e77975820300fe6575b617b24d50
```

本次在用户明确允许后启动了原厂完整系统采集亮灯正样本，采集后恢复
主线 RAM。没有执行升级、固件烧写、配置重置或 `saveenv`。
原厂正常启动会挂载可写配置分区，因此不能把该阶段称作纯 RAM 操作。
所有手动 LED 模式与 GPIO 测试值均已恢复，最后通过重新 RAM 启动清理。
代码与验收工具按路径提交；生成镜像及无关工作区改动不纳入提交，未推送。

## 排查证据与被排除的方向

原厂与主线的 35 个 RTL8372N LED/软件覆盖/引脚寄存器完全一致。
全输出全速率、LED0/1/2 分离输出、参考全局模式均未点亮实际插座。
这些临时模式没有进入正式驱动。

WAN PHY 的 194 个寄存器正反样本全部读齐，只有 8 处差异：BMCR、
千兆协商、间接访问窗口、温度及两项协商配置。没有证据支持将这些
差异作为 LED 修复。U-Boot 下用户也确认不亮，不能当作亮灯正样本。

原厂 `phtest` 的 GPIO 参数按十六进制解析。一条最初使用 `26` 的
测试实际访问了 GPIO38；已恢复其数据为 0、方向为原来的输入，读回
方向寄存器为原值 `0x11`。后续统一使用显式十六进制 GPIO 参数。

关键本地记录：

- `out/wan-led-mainline.json`、`out/wan-led-factory.json`：WAN 194 项对照。
- `out/led-switch-factory.json`：原厂 35 个交换机 LED 寄存器。
- `out/led-soc-factory.json`：原厂 pinmux 和 GPIO 方向/数据。
- `out/port-led-final-build.log`、`out/port-led-final-boot.json`：完成五口实物验收的首版。
- `out/port-led-responsive-build.log`、`out/port-led-responsive-boot.json`：最终提速版。
- `out/port-led-final-state.json`：五灯软硬件状态核对。
- `out/port-led-phy-poll.json`：提速版 PHY 管理读取周期观测。

`port/mainline/led-acceptance/` 保留只读采集、可恢复实验和验收工具，
正式镜像不加载这些诊断模块。
