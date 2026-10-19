# flowtable 硬件卸载的流量计数（2026-10-19）

**现象**：nlbwmon 只记到硬件卸载流量的一部分，短连接几乎不计：2 秒的 iperf 几乎为 0，5 秒的只记到 61–69%。

**原因**（nf_flowtable 通用代码，与 zx279133 驱动无关）
- 卸载到硬件的流，计数只靠 `flow_offload_work_stats()` 读回。`nf_flow_offload_stats()` 要等流的超时走掉十分之一才排一次，TCP 约每 3 秒一次。
- 流结束时 `flow_offload_work_del()` 直接删除硬件规则，不再读最后一次，上次读数之后硬件转发的字节就进不了 conntrack。
- 凡是支持 flowtable 硬件卸载、又开了 conntrack 计数的驱动都会少算短连接。

**修复**：内核 96e5a2413（`net/netfilter/nf_flow_table_offload.c`），`flow_offload_work_del()` 删规则前先调一次 `flow_offload_work_stats()`。代价是每条流结束时多读一次计数。上游新内核有没有修过同样的问题，没查。

**实测**（整包固件，槽位 0 序号 112；停掉 nlbwmon，iperf3 单流；conntrack 记的是 IP 层字节，比 iperf 载荷多约 4.6%，即全部计入）

| 连接时长 | 上传：iperf → conntrack | 下载：iperf → conntrack |
|---|---|---|
| 2 秒 | 591 MB → 618 MB | 487 MB → 510 MB |
| 5 秒 | 1.475 GB → 1.545 GB | 1.213 GB → 1.271 GB |
| 10 秒 | — → 3.085 GB（iperf 的数字没取到） | 2.423 GB → 2.539 GB |

**另一个现象相近的问题**（ImmortalWrt 仓库 40306de847，不在内核里）：nlbwmon 一条都不记、`conntrack -E` 没有输出。原因是 kmod-nft-fullcone 每次防火墙重载都注销每个 netns 唯一的 conntrack 事件通知器，把 ctnetlink 的冲掉。它和硬件卸载无关：驱动在流存在期间会把硬件计数同步进 conntrack。
