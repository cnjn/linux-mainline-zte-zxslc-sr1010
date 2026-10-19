# WANID 按需分配（多 WANID 支持）

2026-10-13 开始。状态：厂商侧静态分析完成；主线代码在编译机上完整编译通过（W=1 和 checkpatch --strict 都无告警）。**T1–T5 上板通过**（§6）。T5 暴露了一处回归，已修复，修复版在 kexec 测试内核上验证过；但测试机 flash 里的固件（`candidates-wanid-20261013`）还是修复前的版本。

相关文档：`NPPT_FORWARDING.md` 的 “WANID Hardware Contract” 一节（WANID 表 128 位布局、两位模式字段的取值），`TAGS_MAC_ACCEPTANCE.md` 的 “Policy change: one address for WAN and LAN” 一节（源 MAC 限制的由来）。本文不重复那两处已经写清的内容。

## 1. 要解决的问题

改动前，主线快表卸载只用两项 WANID：上行（LAN→WAN）固定用 WANID 0，下行（WAN→LAN）固定用 WANID 16。WANID 项里有源 MAC（`m_cpumac`）、SNAT 源地址（`m_sip`）、PPPoE SID 和 PPPoE 改写模式，所以所有流只能共用一组值，由此带来四条限制：

| 限制 | 改动前的代码表现 |
|---|---|
| 只能有一个 SNAT 源地址 | `zx279133_flow_snat_get()` 遇到第二个地址返回 `-EOPNOTSUPP` |
| 只能有一个 PPPoE session | `zx279133_flow_pppoe_get()` 遇到第二个 SID 返回 `-EOPNOTSUPP` |
| PPPoE 和 IPoE 不能并存 | PPPoE 生效后，LAN→WAN 的非 PPPoE 流被拒绝；WAN→LAN 的流一律按 PPPoE pop 处理 |
| 源 MAC 必须等于 WAN MAC | `zx279133_flow_parse()` 拒绝其他源 MAC，所以 LAN 用独立 MAC 时，下行流全部留在软件路径 |

## 2. 厂商实现（np.ko / switch.ko 静态分析）

样本：`vendor-reference/sr1010-vendor-runtime/modules/files/kmodule/{np,switch}.ko`，vermagic 5.4.196。工具：radare2 6.1.6（`-e bin.relocs.apply=false`，否则 `bl` 显示为 `invalid`），重定位表用 Python 直接解析 ELF。下面的地址用 `nm` 的节内偏移；r2 的虚拟地址要再加 `0x08000000 + 0x64`。

### 2.1 调用链

厂商内核里的 FFE（编进内核镜像，`ffe_*` / `npu_cap_*`，dmesg 显示 `ffe max flow cnt is 40000`）负责学习流，然后经 `switch.ko` 的 `l3_hardfastReg` → `sw_acl_l3_hardfast_session_add` → `np.ko` 的 `zte_api_fast_l3_session_add` → `fast_hashinfo_set()`（`0xa6564`）生成快表条目。

**更正前一轮的推测**：`zte_api_sw_wan_port_add/del/change`（→ `tm_add_wan_port`）和 `zte_api_sw_double_wan_set`（→ `tm_sw_double_wan_set`）配置的是**物理端口角色**：`lan_up_port`、`nppt_glb_lan_up_port_set`、`global_wanport_*`、`tm_port_wanpara_set`、`g_pon_work_mode`，也就是把 LAN 口改作上联口。它们不分配 WANID，与本项无关。

### 2.2 `fast_hashinfo_set()` 中的 WANID

响应缓冲区在栈上 `x29+0x78`，key 在 `x29+0x98`。证据：`key[15]` 写入协议号（`[x29,0xa7]`），与主线 `zx279133_fast_key_build()` 一致。WANID 镜像（4 个字）在 `x29+0xb4`。

1. `wanid_get_wanid_by_index(dir, index)`：`wanid = index | (dir == 1) << 4`，`index` 取自 session 的 `+0xb5`（即 `nbifN` 的 N）。
2. 如果修改标志与 `0x30000000`（隧道）有交集：`wanid += 8`，并且按 session 类型改用 `wanid_get_wanid_by_mac()`。
3. 普通路由流（不走隧道分支、`[session+0x18]`，即 action 为 0）：**每条流都调用 `wanid_set_mac(wanid, session MAC)`**，把源 MAC 写进该 WANID 在软件库里的那一项（32 项 × 68 字节）。所以厂商的模型就是“WANID 的 MAC 跟着流走”，两个方向（`index` 与 `index|16`）各自写自己的 MAC。
4. `wanid_database_get(wanid)` 取出缓存镜像。随后响应 byte 23 的 bit 4:0 写入 WANID（`bfxil w0, w1, 0, 5` → `[x29,0x8f]`，`0x8f - 0x78 = 23`）。
5. SNAT（修改标志 bit 8）：response byte 29 bit 7 置 1，WANID 镜像 `m_sip` = 全局变量 `zte_onu_sip`。
6. PPPoE push（bit 0x11）：`m_pppoe = 1`，SID = 全局变量 `zte_session_id`，长度 `+8`；pop（bit 0x10）：`m_pppoe = 2`，长度 `-8`。
7. 构造完成后与缓存镜像比较（`wanid_database_wanid_is_equal`），只有变化时才 `wanid_table_set()` 写硬件。

`zte_onu_sip` 和 `zte_session_id` 都在 `np.ko` 的 `.bss` 里。全部 `.rela.*` 中只有 `fast_hashinfo_set()` 引用它们，而且只有读操作；其他模块的字符串里也没有这两个名字。因此**厂商的 SIP 和 SID 是单一全局值**：厂商同样是“每个逻辑 WAN 一对 WANID，同一 WAN 的所有流共用一组值”。这两个变量具体从哪里被写入（例如经由 `kallsyms_lookup_name`），在现有样本里没有找到。

### 2.3 响应字段（与主线 `zx279133_fast_response_build()` 逐项对照）

| 位置 | 厂商写入条件 | 主线 |
|---|---|---|
| byte 0–1 | bit 15：源端口 | LAN 入口写源端口 |
| byte 4–7 | bit 7：目的 IPv4；byte 30 bit 0 | 同 |
| byte 8–13 | bit 0：目的 MAC；byte 30 bit 4 | 同 |
| byte 14–15 | bit 14：目的端口；byte 30 bit 2 | WAN 入口写目的端口 |
| byte 22 | bit 5 / bit 23：一个字节字段，byte 29 bit 6 | 不用 |
| byte 23 bit 4:0 | WANID | **本次改为分配到的 WANID** |
| byte 29 bit 7 | SNAT（源地址取自 WANID `m_sip`） | 同 |
| byte 30 bit 3 | 源 MAC 取自 WANID `m_cpumac` | 始终置 1 |
| byte 30 bit 5 | 见下 | 始终置 1 |
| byte 30 bit 7:6 | VLAN 动作：1 push（+4），2 pop（−4） | LAN 入口置 bit 7（剥 RTL8372N 私有 tag） |
| byte 31 bit 1:0 | action（0 路由，1 local fast） | PPPoE push 用 1 |

**byte 30 bit 5 的分支**：这一位为 0 时，厂商用 `get_wanid(port)` 的结果覆盖 byte 23；为 1 时，把同一个值写进 WANID 镜像的 bit 117:112（即 `NPPT_FORWARDING.md` 里记为 `m_rsv` 的字段）。`get_wanid()` 读的是 `.data+0x16450` 处 24 项 × 12 字节的按端口表，值在 0..47 之间（例如端口 0 → 0/1，端口 6 → 16/17，端口 8 → 32/33），超出 5 位 WANID 的范围，所以它**不是 WANID 表**。主线一直把 bit 5 置 1、不写 `m_rsv`，并且已经在板上验证过 WANID 0/16 的 SNAT 和 PPPoE。`m_rsv` 的含义仍然未知；本次改动保留它原来的值（读-改-写，并在释放时恢复）。

## 3. 主线设计

### 3.1 WANID 槽位（`zx279133-np.c`）

- `struct zx279133_wanid_profile { mac, sip, sid, pppoe }`：一条流对 WANID 项的全部需求。`sip == 0` 表示这条流不做 SNAT。
- `eth->wanid[32]`：每项记录配置、首次借出时保存的 4 字原始镜像、用户数和 SNAT 用户数，由已有的 `wanid_lock` 保护。
- `zx279133_wanid_get(eth, downstream, profile, &wanid)`：
  - 上行在 0..15 里找，下行在 16..31 里找，与厂商 `index | dir << 4` 的分半一致。
  - 已借出且相容的项直接复用。相容的条件是 MAC、PPPoE 模式、SID 都相同，并且 SIP 也相容：这条流不做 SNAT、该项当前没有 SNAT 用户，或者 SIP 相同。
  - 否则取**最低号的空闲项**：先读出原始镜像保存，再写入配置。所以只有一组配置时仍然落在 WANID 0/16，单 WAN 场景的硬件状态与改动前一致。
  - 已借出、但没有 SNAT 用户的项，遇到新的 SNAT 流时可以就地改写 SIP。
  - 一半用完时返回 `-ENOSPC`，这条流留在软件路径。
- `zx279133_wanid_put(eth, wanid, snat)`：最后一个用户归还时写回保存的原始镜像。
- 写入只改 SID（bit 31:16）、SIP（仅当配置里有 SIP）、MAC（bit 111:64）和 `m_pppoe`（bit 123:122）。L3 MTU、`m_rsv`、其他隧道模式位都沿用保存的镜像。
- `zx279133_program_wanid_cpu_mac()`（改 WAN MAC 时调用）：对于已借出的项，只更新保存的镜像，不打断正在使用它的流；归还时再写回新的 CPU MAC。
- 删除了 `zx279133_program_wanid_sip()` / `zx279133_program_wanid_pppoe()`，它们只有快表卸载在用。

### 3.2 快表卸载（`zx279133-offload.c`）

- 每条流按自己的配置借一项 WANID：
  - 源 MAC 取改写后以太网头的 `h_source`。
  - 上行 SNAT 时带上转换后的源地址。
  - 上行 PPPoE 时 `pppoe = push` 并带上 SID，下行 PPPoE 时 `pppoe = pop`。
  - `entry->wanid` 写进响应 byte 23。
- 去掉了“源 MAC 必须等于 WAN MAC”的检查和对应的 `dev_warn_once`。
- **下行 PPPoE 的判断**：nf_flowtable 的下行规则只匹配 META ingress ifindex（PPPoE 解析后的下层网卡）和 VLAN，不带任何 PPPoE 信息（`nf_flow_rule_match()` 只为 `ETH_P_8021Q` 封装加匹配），PPPoE 只以 `FLOW_ACTION_PPPOE_PUSH` 的形式出现在上行规则里。所以：
  - 每条上行流按“回复方向的硬件 key”（转换后元组交换源和目的）进入 `reply_flows` 索引（`rhashtable_lookup_insert_fast`，重复 key 不入索引）。
  - 下行流用自己的 key 去查：找到对端，就跟随对端是否走 PPPoE。
  - **找不到对端时一律返回 `-EAGAIN`**。这时下行方向的报文继续走软件 flowtable，`flow_offload_refresh()` 大约每秒触发一次 `nf_flow_offload_add()`，重新下发两个方向；已在硬件里的上行方向按 cookie 幂等返回 0，下行方向此时能查到对端。
  - 最初的实现是"找不到对端时，若当前有 PPPoE 上行流就推迟，否则按 IPoE 安装"。T5 证明这样不行（§6）：WAN 发起的连接，下行方向先到，如果当时恰好没有别的 PPPoE 流，它会被按 IPoE（不 pop）装进硬件。而硬件按内层元组匹配 PPPoE 帧，于是把 PPPoE 帧原样转给 LAN，连接直接断掉。旧代码里所有下行流共用 WANID 16，回复方向的 PUSH 一装上就把 WANID 16 改成 pop 模式，这种情况被"顺带"修正了；拆分配置之后这层顺带修正没有了，所以必须改成不猜。
- 代价：LAN 发起的连接（ORIGINAL 是上行）不受影响；WAN 发起的连接（端口转发、IPv6 入站），下行方向要先走大约 1 秒软件，再进硬件。在 2.5G 链路上实测，第 1 秒 1.2 Gbit/s，之后 2.34 Gbit/s。
- 只用 tc flower 单独下发下行规则、没有对应上行规则的用法，下行会一直被推迟。这是已知限制。

### 3.3 回读接口（`/sys/kernel/debug/zx279133-wanid`）

为了验收，驱动新增了一个只读 debugfs 文件（`CONFIG_DEBUG_FS`），每行对应一项 WANID：先是硬件里的 4 个原始字，然后是解码后的 L3 MTU、PPPoE SID、SIP、MAC、`m_pppoe`，最后是驱动记录的借出数和 SNAT 用户数。读取时持有 `datapath_lock` 和 `wanid_lock`，硬件未就绪时只输出 `hardware not prepared`。

刚开机、还没有流时（2026-10-13，测试机）：

```
 0 000007cc 00000000 21b9c13a 00007c7d mtu 1996 sid 0 sip 0.0.0.0 mac 7c:7d:21:b9:c1:3a pppoe 0 users 0 snat 0
 1 000007cc 00000000 21b9c13b 00007c7d mtu 1996 sid 0 sip 0.0.0.0 mac 7c:7d:21:b9:c1:3b pppoe 0 users 0 snat 0
 2 000007cc 00000000 00000000 00000000 mtu 1996 sid 0 sip 0.0.0.0 mac 00:00:00:00:00:00 pppoe 0 users 0 snat 0
 ...
16 000007cc 00000000 21b9c13a 00007c7d mtu 1996 sid 0 sip 0.0.0.0 mac 7c:7d:21:b9:c1:3a pppoe 0 users 0 snat 0
```

与 `NPPT_FORWARDING.md` 记录的初始化一致：32 项的 L3 MTU 都是 1996，WANID 0/16 是 WAN MAC，WANID 1 是 MAC+1，其余为 0。

### 3.4 不在本项范围内

- 多个物理 WAN 或带 VLAN 的 WAN：`zx279133_flow_dev_port()` 仍然只认原生 WAN 网卡；解析器拒绝 `FLOW_ACTION_VLAN_PUSH/POP`。正如 `NPPT_FORWARDING.md` 所说，换出口端口或加运营商 VLAN 要靠 WANID 以外的字段和表。
- 隧道（DS-Lite / VXLAN / GRE）的 `+8` bank 偏移和按 MAC 反查 WANID 的规则。

## 4. 构建

编译机 `ssh -p 2022 192.168.223.19`。为了不影响 ImmortalWrt 原地编译用的外部内核树（`~/workspace/linux-mainline-zte-zxslc-sr1010/linux-6.18.38`，`.config` 就在源码树里），改用独立的 worktree 和输出目录：

```sh
cd ~/workspace/linux-mainline-zte-zxslc-sr1010/linux-6.18.38
git worktree add --detach ~/workspace/wanid-profiles 8aa6f4607
cd ~/workspace/wanid-profiles && git apply /tmp/wanid.patch
mkdir -p ~/workspace/wanid-build
cp ~/workspace/linux-mainline-zte-zxslc-sr1010/linux-6.18.38/.config ~/workspace/wanid-build/
# .config 里的 EXTRA_FIRMWARE_DIR 是相对源码树的路径，换到 worktree 后要改成绝对路径
sed -i "s#^CONFIG_EXTRA_FIRMWARE_DIR=.*#CONFIG_EXTRA_FIRMWARE_DIR=\"$HOME/workspace/immortalwrt/target/linux/zte/zx279133/firmware\"#" ~/workspace/wanid-build/.config
# host/bin 里有 bc，timeconst.h 要用
export PATH=~/workspace/immortalwrt/staging_dir/toolchain-aarch64_cortex-a53_gcc-14.3.0_musl/bin:~/workspace/immortalwrt/staging_dir/host/bin:$PATH
export STAGING_DIR=~/workspace/immortalwrt/staging_dir
M="make O=$HOME/workspace/wanid-build ARCH=arm64 CROSS_COMPILE=aarch64-openwrt-linux-musl- -j24"
$M olddefconfig && $M Image modules
$M W=1 drivers/net/ethernet/zte/
git diff | perl scripts/checkpatch.pl --no-tree --strict -
```

结果（2026-10-13，基线提交 8aa6f4607，gcc 14.3.0 musl）：

- `Image modules`：rc=0；zte 目录无 error/warning。唯一的告警是工具链包装器自动加的 `-I$STAGING_DIR/usr/include` 目录不存在（`-Wmissing-include-dirs`），与代码无关。
- `W=1 drivers/net/ethernet/zte/`：无告警。
- `checkpatch.pl --strict`：0 errors、0 warnings、0 checks（687 行）。
- 产物：编译机 `~/workspace/wanid-build/arch/arm64/boot/Image`。还没有打包成 FIT，也没有上板。

## 5. 上板验收计划

每一项都要看硬件证据（WANID 表回读、抓包、快表计数），不能只看 `[HW_OFFLOAD]`。

1. **单 WAN 回归**：IPv4 NAT、IPv4/IPv6 over PPPoE 各跑一遍。回读 WANID 表，确认只用了 0/16，内容与改动前相同；流删光后 WANID 0/16 恢复原值。
2. **LAN 独立 MAC**：恢复 `lan_cpu` 的 `factory_lan_mac`，WAN 下载。确认下行流进硬件；WANID 16 的 `m_cpumac` 是 LAN MAC；在 LAN 侧抓包，源 MAC 是 LAN MAC；WAN 入 CPU 的包数接近 0（参照 `TAGS_MAC_ACCEPTANCE.md` 的测法）。
3. **两个 SNAT 地址**：WAN 上加第二个地址，用 nft 把两组 LAN 主机分别 SNAT 到两个地址。确认 WANID 0/1 的 `m_sip` 分别是两个地址，WAN 侧抓包源地址正确，并且两边的流互不影响。**这是第一次在硬件上使用 WANID 1..15。**
4. **PPPoE 和 IPoE 并存**：同一根 WAN 线上同时跑 PPPoE session 和 IPoE（需要对端配合）。确认两类上行流分别用不同的 WANID（`m_pppoe` 分别为 1 和 0），下行流分别为 pop 和不 pop。
5. **WAN 发起的 PPPoE 连接**：PPPoE 下做 DNAT 端口转发。确认第一次下发时下行方向返回 `-EAGAIN`，约 1 秒内两个方向都进入硬件，数据正确。
6. **生命周期**：删除 flowtable、重启 pppd、改 WAN MAC、`reboot -f`。确认 WANID 镜像全部恢复，没有 `failed to restore WANID` 告警，没有残留状态。

测试设备是 OpenWrt 测试机 `ssh -p 2022 root@192.168.174.5`（ImmortalWrt 25.12.1，内核 6.18.38+）。测试需要换内核，可能影响它的网络，须事先确认。

## 6. 上板结果（2026-10-13）

固件：编译机 `~/workspace/release/candidates-wanid-20261013/`，`sr1010-owrt.bin` 的 sha256 为 `40a034bb1fce8487ce1d4e770b330dbd33a60ddbc6d09a5c110d76cd4a14b817`。它是 `candidates-kexec-20261013` 加上本文的改动、debugfs 回读接口和 `zx279133_eth_shutdown()`（见 `KEXEC_TEST_LOOP.md`），内核配置不变。由 Claude 用 sysupgrade 写入 slot 1（序号 56）；slot 0（序号 55，无 WANID 改动）留作后备。

流量：mac 用 `curl --interface en0`（走测试机 LAN）从 `mirrors.ustc.edu.cn` 下载 Debian netinst ISO，限时 15 秒。测试机把 LAN 地址 SNAT 成 WAN 地址 `192.168.174.5`。编译机上的 HTTP 服务从测试机 WAN 侧访问不到（编译机只开放了转发进来的 2022 端口），所以改用公网镜像站。

**对照组**（slot 0，无 WANID 改动）：连接 `[HW_OFFLOAD]`，硬件计数上行 3570 包、下行 25944 包 / 35.2 MB，9.19 MB/s。

### T1 单 WAN 回归：通过

- 连接 `[HW_OFFLOAD]`，下载成功（7.8 MB/s，镜像站速度有波动）。
- 下载期间的 WANID 表：

```
 0 000007cc c0a8ae05 21b9c13a 00007c7d mtu 1996 sid 0 sip 192.168.174.5 mac 7c:7d:21:b9:c1:3a pppoe 0 users 1 snat 1
16 000007cc 00000000 21b9c13a 00007c7d mtu 1996 sid 0 sip 0.0.0.0 mac 7c:7d:21:b9:c1:3a pppoe 0 users 1 snat 0
```

  只借出了 WANID 0/16，内容与改动前的固定写法相同。
- 连接结束后两项都已归还，并恢复成原始镜像（WANID 0 的 SIP 回到 0），dmesg 无告警。

### T2 LAN 独立 MAC：通过

把 `br-lan` 的 MAC 临时改成出厂 LAN 地址 `7c:7d:21:b9:c1:3b`（DSA 端口仍是 `…:3a`），并设置 180 秒后自动改回。

- 连接 `[HW_OFFLOAD]`，硬件计数下行 27825 包 / 37.8 MB，9.21 MB/s，与对照组持平。改动前，这种配置下的下行流会被 `flow offload refused: source MAC ...` 拒绝。
- WANID 表：上行仍是 WANID 0（WAN MAC + SNAT 地址）；下行借出 WANID 16，**装入的是 LAN MAC**：

```
16 000007cc 00000000 21b9c13b 00007c7d mtu 1996 sid 0 sip 0.0.0.0 mac 7c:7d:21:b9:c1:3b pppoe 0 users 1 snat 0
```

- 在 mac 的 `en0` 上抓到的下行数据帧：`7c:7d:21:b9:c1:3b > d0:11:e5:c5:cb:4a ... 202.141.176.110.80 > 192.168.5.194`。这在硬件上证实了**下行帧的源 MAC 取自 WANID 的 `m_cpumac`**（此前只有厂商 `fast_hashinfo_set()` 的静态证据）。
- 改回 MAC 后，WANID 16 恢复成原始镜像。

**教训**：MAC 改回 `…:3a` 后，mac 经 LAN 有一小段时间不通，因为它的 ARP 缓存还指向 `…:3b`，而 `…:3b` 已经不是任何端口的地址。临时补救是 `bridge fdb add 7c:7d:21:b9:c1:3b dev br-lan self local`，让发往旧地址的帧照常被接收（重启后消失）。测试机上没有 `arping`，发不了免费 ARP。下次改 LAN MAC 前，先加好这条本地表项。

### 测试拓扑（2026-10-14）

测不了上游，所以把测试机 WAN 口改接到一台 Ubuntu 22.04 笔记本（`enp2s0`，2.5G），笔记本经 Wi-Fi 上网。笔记本上全部是运行时配置：

- 接替原来的上游：`enp2s0` 配 `192.168.174.254/24`，用 dnsmasq（只开 DHCP）把 `192.168.174.5` 发给测试机（测试机 WAN 是 `proto dhcp`），经 `wlo1` 做 masquerade。测试机配置不用改。
- PPPoE 服务端：`pppoe-server -k`（rp-pppoe 3.12，`apt-get download` 后解到 `/tmp` 里运行，因为当时 unattended-upgrades 占着 dpkg 锁），不认证，服务名 `zx279133-test`，本端 `10.64.0.1`。
- 测试目标：dummy 接口上的 `10.64.200.1`（经 IPoE 到达）和 `10.64.100.1`（经 PPPoE 到达），各跑一个 iperf3。
- 测试机：新增 `wanppp`（`proto pppoe`，`defaultroute 0`），加入 wan 区；运行时路由 `10.64.100.0/24 dev pppoe-wanppp`。
- LAN 侧发起端：mac 的 USB 网卡 `en8`（`192.168.5.146`，2.5G），临时路由 `10.64.0.0/16 via 192.168.5.1`。

### T3 两个 SNAT 地址：通过

去 `10.64.200.1` 的流量在测试机上 SNAT 成 `10.64.1.2`（单独的 nft 表，优先级 99，先于 fw4 的 masquerade；笔记本把 `10.64.1.0/24` 静态路由回 `192.168.174.5`，不需要 ARP），去 `192.168.174.254` 的流量照常 masquerade 成 `192.168.174.5`。两路同时跑 12 秒：

```
 0 ... sip 192.168.174.5 ... pppoe 0 users 2 snat 2
 1 ... sip 10.64.1.2     ... pppoe 0 users 2 snat 2
16 ... sip 0.0.0.0       ... pppoe 0 users 4 snat 0
```

两路分别为 1176 和 1177 Mbit/s，合计 2.35 Gbit/s，基本是线速；硬件逐帧计数，平均每包约 1520 字节。这是 **WANID 1..15 第一次在硬件上使用**，两条流的回包都按各自的 SNAT 地址回来了。流结束后 WANID 1 恢复成原始内容（CPU MAC+1 `…:3b`，SIP 0）。

### T4 PPPoE 与 IPoE 并存：通过

两路同时跑：IPoE 去 `10.64.200.1`，PPPoE 去 `10.64.100.1`。

```
 0 000107cc 0a400002 ... sid 1 sip 10.64.0.2     ... pppoe 1 users 2 snat 2
 1 000007cc c0a8ae05 ... sid 0 sip 192.168.174.5 ... pppoe 0 users 2 snat 2
16 000007cc 00000000 ... 08007c7d ... pppoe 2 users 2
17 000007cc 00000000 ... 00007c7d ... pppoe 0 users 2
```

上行按 PPPoE 与否借到不同的 WANID（push 带 SID）；下行按"找对应的上行流"分出 pop 和不 pop，两种都判对了。改动前这种组合不能卸载：PPPoE 生效后，LAN→WAN 的非 PPPoE 流会被拒绝，所有下行流也会被当成 PPPoE pop。修复后重跑：IPoE 上传 2346 Mbit/s，同时 PPPoE 下载 1396 Mbit/s，分配同上。

### T5 WAN 发起的 PPPoE 连接：修复后通过

测试机上加一条端口转发：PPPoE 地址的 15201 端口转到 mac 的 `192.168.5.146:5201`。笔记本经 PPPoE 连进来，向 mac 上传 10 秒。

- **(a) 当时没有其他 PPPoE 流，修复前**：iperf3 报 `control socket has closed unexpectedly`。下行被按 IPoE 装进 WANID 16（`pppoe 0`），PPPoE 帧被原样转发到 LAN（原因见 §3.2）。**这是本次改动引入的回归。**
- **(b) 已有一条 PPPoE 上行流，修复前**：下行先被推迟，第 1 秒 984 Mbit/s，之后 2.29–2.34 Gbit/s，pop 装在 WANID 16。这在硬件上证实了"推迟、由 refresh 重试"的机制有效。
- **(a) 修复后**（kexec 进修复版测试内核）：第 1 秒 1224 Mbit/s（软件），之后稳定在 2338–2349 Mbit/s，合计 2.58 GB；下行 pop 在 WANID 17。

### 原来就有的问题（与本次改动无关）

用 slot 0（没有 WANID 改动）做 A/B 对照，以下现象完全相同：

- **PPPoE 上行（push）不走硬件**：上传约 800 Mbit/s，测试机软中断明显（约 800 tick / 8 秒）；conntrack 计数平均每包约 17.7 KB，是 CPU 上 GRO 合并后的 skb。作为对比，IPoE 上传 2353 Mbit/s，软中断几乎为 0。`NPPT_FORWARDING.md` 里 PPPoE 上行 0.93 Gbit/s、当时归因为虚拟机接收上限的那次结果，很可能是同一个原因。push 表项是装进去了（`[HW_OFFLOAD]`，WANID 回读正确），但报文没有命中，原因未查。
- **PPPoE 下行（pop）走硬件，但计数器不准**：下载 2335–2339 Mbit/s，CPU 几乎不参与，但 conntrack 里下行方向只计了 1–2 个包。

### 未测

- **T6 生命周期**：flowtable 删除、改 WAN MAC、`reboot -f` 后的恢复。T1–T5 已覆盖"流结束后归还并恢复原始镜像"。
- 带运营商 VLAN 的 WAN、多个物理 WAN：不在本项范围内（§3.4）。
