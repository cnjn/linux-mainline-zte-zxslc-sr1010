# SR1010 网口 LED 实现索引

更新：2026-10-20。按用户确认的机壳编号修正了 LAN 接口及 LED 编号。
既有插座的交换端口与 GPIO 配对保持不变，新编号尚未进行新镜像逐口插拔验收。
2026-09-13 的历史实物证据和边界见 [PORT_LED_ACCEPTANCE.md](PORT_LED_ACCEPTANCE.md)。

## 硬件接线

原厂板级表和实物高低电平测试共同确认，五个插座灯使用 SoC GPIO，
全部低电平点亮：

| 插座 | 交换芯片端口 | GPIO | 主线接口 |
| --- | --- | --- | --- |
| WAN | 独立 WAN MAC | 26（gpio1[10]） | eth0 |
| LAN1 | 4 | 2（gpio0[2]） | lan1 |
| LAN2 | 5 | 13（gpio0[13]） | lan2 |
| LAN3 | 6 | 68（gpio4[4]） | lan3 |
| LAN4 | 7 | 69（gpio4[5]） | lan4 |

旧版编号与当前物理编号相反：旧 lan1/lan2/lan3/lan4 对应新
lan4/lan3/lan2/lan1。保留旧网络配置升级时，需要迁移物理接口引用。
OpenWrt 的 SR1010 DTS 包含公共主线 DTS，因此也继承此次修正。

## 代码位置

- `linux-6.18.38/arch/arm64/boot/dts/zte/zx279133-sr1010.dts`：
  五个 gpio-leds 节点，标签为 `sr1010:wan` 和 `sr1010:lan1..4`。
- `port/mainline/zxdbg.fragment`：启用 LED trigger 和 netdev trigger。
- `port/mainline/initramfs/init`：自动启用四个 LAN 口，将灯绑定到对应
  netdev，默认只启用 link 指示，避免轮询交换机 MIB 统计。
- `linux-6.18.38/drivers/net/phy/zx279051.c`、
  `linux-6.18.38/drivers/net/ethernet/zte/zx279133-rtl8372n.c`：
  通过 PHYLIB 已有 get_next_update_time 回调，将链路轮询周期设置为 250 ms。
- `port/mainline/led-acceptance/`：原厂/主线采集、可恢复实验和运行态验收工具。

## 行为与边界

连接后亮对应网口灯，断开后熄灭，机身红白状态灯保持原有实现。
五个插座的映射、极性和实际插拔均已由用户确认。
最后已恢复主线 RAM 镜像，未刷写固件；代码与验收工具按路径提交。

轮询的是 PHY 链路状态；数据包收发继续使用原有中断/NAPI。
原厂已恢复的 PHY 管理线程同样轮询，名义间隔为 100 ms。
目前没有已验证的链路 IRQ 接线和处理协议，不能用 IDM 收发中断替代。
250 ms 是软件检查周期，插线后的自动协商和调度时间仍然存在。

此前尝试的 RTL8372N 自动 LED 模式不是实际插座灯的控制路径，
均已恢复，未纳入正式驱动。
