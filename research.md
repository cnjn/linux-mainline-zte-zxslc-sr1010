# Research: 截至 2026-08-21 的 Linux 官方主线/稳定/LTS 状态与 6.18 能力边界

## Summary

截至 2026-08-21，kernel.org 列出的当前 mainline 是 **7.2（2026-08-16）**；仍单列维护的 stable 分支是 **7.1.9（2026-08-19）**；6.18 已是 LTS，最新为 **6.18.45（2026-08-19）**。因此 **6.18.38 不是当前 mainline**，而是 6.18 LTS 稳定维护线中的一次补丁版本（2026-07-04），且已落后于 6.18.45。

Linux 6.18 本身已有 Rust Binder、SLUB sheaves、swap table、dm-pcache、AccECN、TCP PSP、BPF 签名基础设施、namespace file handle 等能力，但其中多项是可选或初始实现；它明确不再包含 bcachefs，也不会通过 6.18.y 稳定更新获得 6.19—7.2 的新功能。

## Findings

1. **当前官方版本状态（快照日：2026-08-21）** — kernel.org 首页显示 mainline **7.2 / 2026-08-16**，stable **7.1.9 / 2026-08-19**；LTS 最新点版本依次为 **6.18.45、6.12.104、6.6.152、6.1.183、5.15.216、5.10.265**（均为页面当时所列最新版本）。这里的“stable 7.1.9”是仍在单独维护的上一条稳定分支；7.2 是 Linus 主线树最新正式版，在广义上当然也是已发布的稳定版。[kernel.org 首页](https://www.kernel.org/) [机器可读 finger banner](https://www.kernel.org/finger_banner)

2. **LTS 生命周期** — 官方 active releases 页面把 6.18 列为 longterm：2025-11-30 发布，由 Greg Kroah-Hartman 与 Sasha Levin 维护，预计 EOL 为 **2028-12**。其余在役 LTS 的预计 EOL：6.12 为 2028-12；6.6、6.1 为 2027-12；5.15、5.10 为 2026-12。官方同时强调 EOL 是 projected，可能调整。[Active kernel releases](https://www.kernel.org/releases.html)

3. **6.18.38 的准确归属** — `X.Y.Z` 是 stable/longterm 补丁版本命名；6.18.38 是 **6.18.y LTS 线的第 38 个稳定补丁版本**，发布日期为 **2026-07-04**。它既不是 Linus 当前 mainline，也不是“6.18 主线的新功能版”；当前同线已到 6.18.45。官方文档说明 `6.x.y` 是 `-stable` 内核，补丁直接针对基础 `6.x`。[6.18.38 所在官方归档](https://www.kernel.org/pub/linux/kernel/v6.x/) [6.x 发布说明](https://docs.kernel.org/6.18/admin-guide/README.html)

4. **6.18 已有的核心/内存能力** — 6.18 引入或包含 SLUB **sheaves**（每 CPU 数组式缓存以减少分配/释放路径争用）和 **swap table**（每 swap cluster 的缓存值数组，强调局部性、简单数组查找与 RCU 查找保护）。这类能力属于内核内部实现，不能仅凭版本号推断某个发行版配置一定启用。[6.18 slab 文档](https://docs.kernel.org/6.18/mm/slab.html) [Swap Table 文档](https://docs.kernel.org/6.18/mm/swap-table.html)

5. **6.18 已有的存储能力及边界** — `dm-pcache` 可让 DAX/pmem 作为慢速块设备前的崩溃持久高速缓存，带可选数据 CRC、重复元数据和日志式 write-back；但官方 6.18 文档明确写着：**目前只有 write-back、只有 FIFO 淘汰、不支持 table reload，discard 尚属计划项**。XFS 具备在线检查/修复框架，但官方设计文档也明确说明在线 fsck **不是离线 fsck 的完整替代品**。[dm-pcache](https://docs.kernel.org/6.18/admin-guide/device-mapper/dm-pcache.html) [XFS Online Fsck Design](https://docs.kernel.org/6.18/filesystems/xfs/xfs-online-fsck-design.html)

6. **6.18 已有的网络能力及边界** — TCP 可协商 **Accurate ECN (AccECN)**，并有 `tcp_ecn_option`、beacon 等控制项；同时具备 **PSP** 安全协议支持，但 6.18 文档明确限定 PSP **当前仅支持 TCP**，且依赖 NIC/驱动提供密钥与卸载能力，不能理解为任意网卡、任意协议都自动获得 PSP。[6.18 IP sysctl / AccECN](https://docs.kernel.org/6.18/networking/ip-sysctl.html) [6.18 PSP](https://docs.kernel.org/6.18/networking/psp.html)

7. **6.18 已有 Rust 与 Android Binder，但不是“全面 Rust 化”** — v6.18 源码树已含 Rust Binder 实现及其所需的 Rust 内核抽象；同时传统 C Binder 仍保留，Rust 支持也是配置和工具链相关的可选能力。官方 Rust 文档强调抽象覆盖并不完整，缺少的 C API 应先建立安全抽象，而不是让叶子驱动直接调用裸 bindings。[v6.18 Android 驱动源码目录](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/android?h=v6.18) [6.18 Rust general information](https://docs.kernel.org/6.18/rust/general-information.html) [Rust LSM Binder API](https://rust.docs.kernel.org/kernel/security/index.html)

8. **BPF 签名是基础设施，不等于默认强制签名** — 6.18 有加密签名 BPF 程序的初始能力；该机制建立来源/完整性判定，但“是否只允许运行已签名 BPF”仍需 LSM 策略实施，不能把 6.18 描述成默认拒绝所有未签名 BPF。官方 BPF 签名文档明确说明：签名本身只记录 verdict，未签名程序仍按原方式加载，策略由 LSM 决定。[BPF signing 文档](https://docs.kernel.org/next/bpf/signing.html) [6.18 BPF 文档入口](https://docs.kernel.org/6.18/bpf/index.html)

9. **6.18 明确没有 bcachefs** — bcachefs 在 6.17 被标为 externally maintained，6.18 删除了核心代码；官方提交说明其已成为 DKMS 模块，为避免内外版本混淆而移除。因此若本仓库出现 `fs/bcachefs/` 或相关补丁，那是外置/回填代码，不能称为原生 v6.18 mainline 内容。[官方删除提交 `f2c61db`](https://git.kernel.org/torvalds/c/f2c61db29f27) [6.18 文件系统文档目录（无 bcachefs 项）](https://docs.kernel.org/6.18/filesystems/index.html)

10. **6.18.y 不会补进后续主线的新能力** — kernel.org 对 stable/longterm 的定义是从主线回移重要 bugfix；longterm 只接收重要修复，而不是把后续新功能整体带回旧版本。因此 6.18.38/45 可含安全修复、回归修复和必要硬件修复，但不能据此宣称拥有 6.19、7.0、7.1、7.2 的全部功能。需要后续功能时，应显式维护 backport/topic patch，并单独测试，而不是伪装成“纯 6.18”。[Active releases 对 mainline/stable/longterm 的定义](https://www.kernel.org/releases.html) [Stable kernel rules](https://docs.kernel.org/6.18/process/stable-kernel-rules.html)

11. **“源码有”不等于“构建产物有”** — PREEMPT_RT、sched_ext、Rust、BPF、各文件系统和驱动均受 Kconfig、体系结构和工具链约束。例如 sched_ext 需 `CONFIG_SCHED_CLASS_EXT`，API 无稳定性保证；PREEMPT_RT 还要求体系结构选择 `ARCH_SUPPORTS_RT`。审仓库时必须把“v6.18 源码可支持”“当前 `.config` 已启用”“实际硬件/用户态可用”分成三层。[sched_ext](https://docs.kernel.org/6.18/scheduler/sched-ext.html) [PREEMPT_RT 架构要求](https://docs.kernel.org/6.18/core-api/real-time/architecture-porting.html)

## Sources

- Kept: [The Linux Kernel Archives](https://www.kernel.org/) — 当前 mainline/stable/LTS 点版本与发布日期的一手页面。
- Kept: [Active kernel releases](https://www.kernel.org/releases.html) — 发布类型、维护者、LTS 与 EOL 的官方定义。
- Kept: [Linux 6.18 官方归档](https://www.kernel.org/pub/linux/kernel/v6.x/) — 6.18.38 日期、tarball/patch/changelog 的官方索引。
- Kept: [Linux 6.18 documentation](https://docs.kernel.org/6.18/) — 对 6.18 实际接口、限制和配置条件的版本化文档。
- Kept: [Torvalds v6.18 source tree](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/?h=v6.18) — 判断源码是否真正合入的最终依据。
- Dropped: Phoronix、The Register、9to5Linux、Linux Journal、KernelNewbies — 用于发现候选特性，但最终结论尽量改由 kernel.org/git.kernel.org/docs.kernel.org 佐证；它们不是本任务要求的一手来源。
- Dropped: 搜索摘要中未能落到具体 v6.18 源码、提交或版本化文档的宽泛“feature list” — 容易把后续版本、可选配置或计划能力误算成 6.18 已有能力。

## Gaps

- 官方没有一份结构化、穷尽式的“6.18 有/没有”功能矩阵；完整结论仍需以 `git diff v6.17..v6.18`、Kconfig 和目标 `.config` 为准。
- 本简报只界定官方上游基线，尚未判断当前仓库是否对 v6.18.38 做了 vendor/backport 修改；应把仓库 HEAD 与官方 stable tag `v6.18.38`（以及当前 `v6.18.45`）做 commit/diff 比较。
- BPF 签名文档当前在 `docs.kernel.org/next/` 最完整；若需要对 v6.18 的精确 UAPI/配置做合规审计，应直接核对 v6.18 tag 中 `kernel/bpf/`、UAPI 头文件和相关 Kconfig，而不能只依赖最新版文字文档。
