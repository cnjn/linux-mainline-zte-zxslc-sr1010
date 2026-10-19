# 用 kexec 测试内核，不再反复刷机

2026-10-13。目标：基础固件只刷一次，之后每次测试新内核都用 kexec 切换，不经过 U-Boot，不插拔网线，也不写 flash。执行一次 `reboot` 就回到刷进去的内核。

当前状态：**可用，但还要注意几条限制**（§3）。从带第二版 `zx279133_eth_shutdown()` 的固件出发，kexec 2/2 成功（§4）；WANID 修复版就是在 kexec 进去的测试内核上验证的。

## 1. 基础固件

测试机现在跑的是 `candidates-wanid-20261013`（slot 1，序号 56）：kexec 支持、第二版 `.shutdown`、WANID 改动。各版固件都在编译机的 `~/workspace/release/`：

| 目录 | 内容 | 状态 |
|---|---|---|
| `candidates-kexec-20261013` | compat 版 + kexec | 用户手动刷入过；已被覆盖 |
| `candidates-shutdown-20261013` | + 第一版 `.shutdown`（完整 unprepare） | slot 0（序号 55）；**从它出发 kexec 必然复位**，只作后备 |
| `candidates-wanid-20261013` | + 第二版 `.shutdown`、WANID、debugfs | slot 1（序号 56），正在运行 |

kexec 支持的改动（ImmortalWrt f58f49e141 的工作树，未提交，diff 在各目录的 `immortalwrt.diff`）：

- `target/linux/zte/zx279133/config-6.18`：`CONFIG_KEXEC_FILE=y`，`# CONFIG_KEXEC_SIG is not set`，`# CONFIG_CRASH_DUMP is not set`。
- `package/boot/kexec-tools/Makefile`：`kexec` 包的架构列表加上 `aarch64`（上游 kexec-tools 2.0.28 支持 arm64，OpenWrt 的包没有列出）。
- `.config`（不受 git 管理）：`CONFIG_KERNEL_KEXEC=y`、`CONFIG_PACKAGE_kexec=y`、`CONFIG_KEXEC_ZLIB=y`。

内核改动（`linux-6.18.38` 原地树，未提交，见各目录的 `kernel.diff`）：`zx279133_eth_shutdown()`，以及 WANID 的改动（`WANID_PROFILES.md`）。

## 2. 每次测试

```sh
~/zx279133-docs/port/mainline/kexec/kexec-boot.sh <编译机上的 Image 路径>
```

脚本依次执行：

1. 从编译机把 Image 流式传到测试机的 `/tmp/Image-test`，并校验 sha256。
2. 执行 `kexec -s -l /tmp/Image-test`。DTB 用正在运行的那份，因为 `kexec_file_load` 在 arm64 上会自动沿用当前 FDT。
3. 通过 mac（`ssh 192.168.167.93`，走 `en1`，不经过测试机）的 tmux 会话 `sr1010-serial`，在串口 shell 里执行 `sync; kexec -e`。脚本会先等到 shell 提示符出现，因为刚开机时控制台在等回车，提示符出现前输入的内容会丢。
4. 在串口日志里等到 `Please press Enter`，再等 ssh 恢复，最后报告版本、命令行、CPU 数和 dmesg 里的异常。

**测试内核必须用发布目录里的 `kernel.config` 编译**，否则 rootfs 里 `=m` 的模块（`nf_flow_table`、nft、wireguard 等）会加载失败。编译方法见 `WANID_PROFILES.md` §4（同样要改写 `CONFIG_EXTRA_FIRMWARE_DIR`）。

**如果要连续 kexec**（从一个测试内核直接 kexec 到下一个），测试内核要把参数编进自己的命令行，因为 kexec 传入的命令行会被忽略（§3.3）：

```sh
scripts/config --file $O/.config --set-str CMDLINE \
  "console=ttyAMA0,115200n8 root=/dev/mtdblock7 rootfstype=squashfs ro rootwait nosmp panic=10 watchdog.stop_on_reboot=0"
```

`CONFIG_CMDLINE` 不参与模块的 vermagic，所以改它不影响模块加载。尚未实测。

**回到原系统**：在测试机上 `reboot`。

## 3. 首次验证中发现的问题

### 3.1 kexec 时 NPPT 的 DMA 没停：需要 `.shutdown`

以太网驱动原来没有 `.shutdown`，LAN 交换机驱动又一直持有 datapath，所以 kexec 时 NPPT/IDM 照常运行，收到的帧会被 DMA 写进旧内核的接收缓冲区，而新内核可能已经在用那块内存。从没有这个 hook 的固件出发，kexec 1 次成功、1 次在 `Bye!` 后彻底卡死，需要断电。

第一版 hook 调用了 `zx279133_hardware_unprepare()`，即整套"恢复到上电状态"（DMA/BMU/QMG/SE/PPU 的 restore，最后关时钟）。从这版固件出发 kexec 了 3 次，**每次都在 `Bye!` 后一两秒内整板复位**（串口直接出现 SPL 的 `enter bootloader`），与内核镜像无关：OpenWrt 编的 Image 和 worktree 编的 Image 都一样。原因没有深究，可能是某个模块被恢复成启动时的配置后又开始工作。

第二版 hook（现在的版本）只把报文挡在入口：关 XMAC0（LAN）和 XMAC1（WAN）的 RX，停中断和 NAPI，排空 TX，再关两个 XMAC 的 TX，关路由，停 IDM 取 TX 描述符；不做 restore，也不关时钟。从这版固件出发 kexec 1 次，成功。

### 3.2 kexec 后只剩一个 CPU，且不能再次 kexec

```
[  415.839591] psci: CPU1 may not have shut down cleanly (AFFINITY_INFO reports -1)
...
[    5.040917] CPU1: failed to come online
[    5.040922] CPU1: failed in unknown state : 0x0
```

厂商固件报告 PSCIv1.0，但旧内核关闭 CPU1 后，`AFFINITY_INFO` 返回 -1（`PSCI_RET_NOT_SUPPORTED`）；新内核用 `CPU_ON` 唤醒 CPU1，5 秒内没有进入内核（这也让启动多花 5 秒）。之后这个内核会把 CPU1 记为"卡在内核里"，**拒绝任何新的 kexec**（`Can't kexec: CPUs are stuck in the kernel`，`kexec_file_load` 返回 `EBUSY`）。

后果：
- 吞吐、RSS/多队列、RX 双 NAPI 这类和 CPU 数相关的测试不能在 kexec 后的内核上做。
- 不带 `nosmp` 的测试内核之间不能连续 kexec，每次都要先 `reboot` 回到 flash 内核。

可能的调查方向：正常启动时测 CPU 热插拔（`echo 0 > /sys/devices/system/cpu/cpu1/online` 再 `echo 1`）；看厂商 5.4 内核的 SMP 启动方式。

### 3.3 kexec 传入的命令行被忽略

内核配置是 `CONFIG_CMDLINE_FORCE=y`，命令行固定为 `console=ttyAMA0,115200n8 root=/dev/mtdblock7 rootfstype=squashfs ro rootwait`（U-Boot 的 `bootargs` 本来就被忽略）。所以 `--append`、`--command-line` 都不起作用：给的 `nosmp`、`panic=10`、`earlycon`、`watchdog.stop_on_reboot=0` 一个都没生效（看 `/proc/cmdline`，以及看门狗驱动在 5.57 秒才加载，即等 CPU1 超时之后）。要用这些参数，只能编进测试内核自己的 `CONFIG_CMDLINE`（§2）。

### 3.4 其他

- **传统 `CONFIG_KEXEC` 在 arm64 上选不上**：`ARCH_SUPPORTS_KEXEC` 是 `def_bool PM_SLEEP_SMP`，需要系统休眠支持。OpenWrt 的 `KERNEL_KEXEC` 开关因此不生效，构建也不报错。改用 `KEXEC_FILE`，加载时用 `kexec -s`。
- **`kmod-nft-fullcone` 要手动重编**：改了内核配置后它不会自动重编，`package/install` 报 `breaks: kmod-nft-fullcone ... [kernel=...旧哈希]`。处理办法：`make package/network/utils/fullconenat-nft/clean` 后再 `make`。
- **`kexec -e` 不能从 ssh 后台执行**：ssh 会话一关，`nohup ... &` 起的进程也会被杀掉，`kexec -e` 根本没跑。要从串口 shell 执行。
- **看门狗兜不住第一跳**：看门狗驱动调用了 `watchdog_stop_on_reboot()`，kexec 走 reboot 通知链，会在跳转前停掉两个看门狗；新内核要等驱动加载（约 0.5 秒）后才重新启动它们。所以新内核在这之前卡住，没有任何东西能复位板子。
- **`[Firmware Bug]: Kernel image misaligned at boot`**：`kexec_file_load` 没有按 2 MB 对齐放置内核，KASLR 会重定位，没有观察到影响。

## 4. 尝试记录

| # | 出发固件 | 镜像 | 结果 |
|---|---|---|---|
| 1 | kexec 固件（无 hook） | OpenWrt 编，自身 | 成功；单 CPU |
| 2 | kexec 固件（无 hook） | worktree 编，WANID | `Bye!` 后卡死，用户断电 |
| 3 | 第一版 hook | worktree 编，WANID | 新内核打印约 20 行后整板复位，回到 flash |
| 4 | 第一版 hook | OpenWrt 编，自身 | `Bye!` 后立即复位 |
| 5 | 第一版 hook | OpenWrt 编，自身（`--reuse-cmdline`） | 新内核打印版本行后复位 |
| 6 | 第二版 hook | OpenWrt 编，自身 | 成功；单 CPU，CPU1 超时 5 秒 |
| 7 | 第二版 hook | worktree 编，WANID 修复版 | 成功（2026-10-14） |

另有 2 次没有执行：一次是 kexec 后的单 CPU 内核拒绝再次 kexec（§3.2）；另一次是 mac 上的 tmux 服务器不见了，串口会话 `sr1010-serial` 不存在，脚本报 `the console did not take the command`。后者用下面的命令重建会话后正常：

```sh
tmux new-session -d -s sr1010-serial "tio -b 115200 /dev/cu.usbmodem5B140649641"
```

测试机 WAN 改接笔记本期间（`WANID_PROFILES.md` §6），`192.168.174.5` 只能经笔记本访问，脚本里的 `ROUTER` 要改成 `ssh ... -J 192.168.167.100 -p 2022 root@192.168.174.5`。另外，测试内核的 rootfs 模块会报 `Kernel module BTF mismatch detected`，模块照常加载，属于预期之内。

## 偶发：kexec 后卡在等根设备（2026-10-15）

一次 kexec（第 22 次左右，之前刚从厂商系统刷回 slot 1 并启动）后，新内核停在 `Waiting for root device /dev/mtdblock7...`，以太网驱动报 `deferred probe pending … wait for supplier …/partition@100000/nvmem-layout/mac-address`，说明 SPI 闪存没探测出来；同时旧内核报 `CPU1 may not have shut down cleanly`。这时没有用户态，串口 BREAK（tio `ctrl-t b` + `b`）无效（SysRq 未启用），调试串口的 DTR/RTS 也没接复位，只能断电。断电后从 slot 1 正常启动，再 kexec 正常。其余十几次 kexec 都没有出现。
