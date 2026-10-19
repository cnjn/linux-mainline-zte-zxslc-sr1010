# zx-slot：在厂商系统和 ImmortalWrt 两边都能用的槽位切换工具

2026-10-15。用户要求：厂商的 `fw_flashing -v switch` 切换槽位有 bug；`zte-fwslot` 依赖 MTD 的 `offset` sysfs 属性，而厂商 5.4 内核没有这个属性。所以单独写了一个只依赖固定地址的小工具，放在两边共用的 Plugin 分区里。

## 用法

```
zx-slot info          显示两个槽位，以及下次启动哪一个
zx-slot check 0|1     只读，完整校验该槽位（版本头、U-Boot、内核、rootfs 的 CRC）
zx-slot switch 0|1    校验通过后，把该槽位的版本头序号改成"最大序号 + 1"（只能在 ImmortalWrt 上用）
```

**在厂商系统上不要用它切换**：厂商内核读 flash 不可靠（§事故），`switch` 在厂商侧会直接拒绝。要从厂商系统回到 ImmortalWrt，用 `fw_flashing -f /var/tmp/<ImmortalWrt 包>` 重刷 slot 1：它擦除整个目标槽位并写入新头，U-Boot 随后会启动它（2026-10-15 验证两次）。

- ImmortalWrt：`/overlay/zx-slot`
- 厂商系统：`/Plugin/zx-slot`（同一个文件，厂商把 mtd9 "Plugin" 挂在 `/Plugin`）

它只读写两个槽位，从不写固件包：刷固件在厂商上用 `fw_flashing -f /var/tmp/<包>`（厂商 `/tmp` 只有 20 MB），在 ImmortalWrt 上用 `zte-fwslot flash` 或 sysupgrade。

## 实现

- 源码：`zx-slot.c`。版本头格式、CRC、槽位校验沿用 immortalwrt `package/utils/zte-fwslot/src/zte-fwslot.c`。
- 两个槽位的起始地址固定：slot 0 是 `0x600000`，slot 1 是 `0x2f00000`，各 `0x2900000` 字节。版本头的位置随镜像大小变化，要在槽位里逐块扫描。
- 只用 `/proc/mtd` 按名字找设备：ImmortalWrt 有 `firmware-slot1`/`firmware-slot2`，各自从偏移 0 开始；厂商有 `whole flash`（mtd0，128 MB），槽位按固定偏移访问。
- `switch` 直接指定目标槽位，不需要知道当前跑的是哪个；写完后回读确认。目标已经是下次启动槽位时，什么都不写。
- 编译（编译机）：`aarch64-openwrt-linux-musl-gcc -O2 -Wall -Wextra -static zx-slot.c -o zx-slot` 后 strip。用静态 aarch64：厂商用户态是 32 位 ARM，没有 musl 加载器，但内核是 64 位，可以直接运行静态的 aarch64 程序。当前版本 sha256 `36b055adfd1ea763e9d039617301f35b38d2232a5048dcc922011795e79f6abb`。

## 验证（2026-10-15）

| 在哪边 | 操作 | 结果 |
|---|---|---|
| ImmortalWrt | `info`、`check 0`、`check 1` | 与 `zte-fwslot info` 一致；两槽位校验通过，各约 4 秒 |
| ImmortalWrt | `switch 1`（已是下次启动） | 提示已是下次启动，不写 |
| ImmortalWrt | `switch 0` | slot 0 序号 59 → 61，重启进入厂商系统 |
| 厂商 | `info` | 通过 `/dev/mtd0` + 固定偏移正确识别两个槽位 |
| 厂商 | `check 1` | 通过 |
| 厂商 | `check 0`（自己正在运行的槽位） | **报 `root filesystem CRC mismatch`** |
| 厂商 | `switch 1` | slot 1 序号 → 62，重启回到 ImmortalWrt |
| ImmortalWrt | `check 0` | 又通过了 |

所以 flash 里的厂商槽位没有被改动；在厂商系统上校验它自己正在运行的槽位时，rootfs 的 CRC 对不上，原因未查。这不影响切换：两边都是切到"另一个"槽位，而另一个槽位的校验正常。

## 事故：厂商内核读错 flash，切换写到了旧头上（2026-10-15）

第二次进入厂商系统后：

1. `zx-slot check 0`、`check 1` 都报 rootfs CRC 不匹配，连续多次一样；回到 ImmortalWrt 后同样两个槽位都通过。
2. 当时还有一个 `switch -f`（强制切换）选项。用它在厂商侧切到 slot 1 后，`info` 显示 slot 1 的头在 `0x28e0000`，内容是**厂商版本** `V1.0.0.2B5.8000`，`H_SLOT_START=0x2f00000`，序号 64。
3. 在厂商侧用 `dd if=/dev/mtd0` 读第 643 块（`0x5060000`，ImmortalWrt 的头所在），**读出来全是 `0xff`**；而几分钟前 U-Boot 和 ImmortalWrt 都正是用这个位置的头启动的，`fw_flashing` 的日志也写明头在 `0x5060000`。第 703 块（`0x57e0000`）里则是一个很早以前厂商住在 slot 1 时留下的旧头。

也就是说，厂商内核经 `whole flash` 读 slot 1 时，至少有一部分块读到的不是实际内容。扫描跳过了真正的头，落到旧头上，`-f` 就把新序号写进了旧头。

修复：在厂商侧用 `fw_flashing -f /var/tmp/fw.bin` 重刷 ImmortalWrt。它擦除整个 slot 1（旧头也被擦掉），写入新头（序号 64）。之后厂商侧读第 643 块变成了正确的头，第 703 块为 `0xff`。重启后 U-Boot 报 `found valid head @0x2ee0000`、`@0x5060000`，启动了 ImmortalWrt。

之后的改动：删掉 `-f`；通过 `whole flash` 访问时（即厂商侧）拒绝 `switch`，`info` 输出注明结果不可靠。原因（SPI-NAND 的读取方式、ECC 或别的）没有查。当前版本 sha256 `3d7d256e6167f37ab4a18a93927fe96c1a6748720e7f273dc351998adb6e6ee2`。

