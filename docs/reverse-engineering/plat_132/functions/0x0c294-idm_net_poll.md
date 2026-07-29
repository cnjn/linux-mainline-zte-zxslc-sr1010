# 0x0c294 idm_net_poll

## Status

- Status: complete
- Confidence: verified loop bounds, fixed queue index, count packing, RX call
  order, completion callback, NAPI slot/source mapping, and return behavior.
- Size: `0x138` bytes, 78 ARM64 instructions.
- Recovered signature: `int idm_net_poll(struct napi_struct *napi, int budget)`.

## Semantics

This is source-1's poll callback, installed at NAPI slot 1
(`int_info + 0x1a0`) on the IDM netdev. It ignores the passed `napi` argument,
increments raw counter `0x27de4`, and performs at most four iterations.

Every iteration reads `cpu_net_ops + 0x10` (`idm_get_cpu_rx_cnt`) for fixed
queue index 8 before testing the remaining budget. A zero packed count or a
nonpositive remaining budget ends the loop. Low 16 bits are normal count and
high 16 bits are jumbo count. Each count is independently capped at half of the
current remaining budget, then dispatched as:

```c
idm_net_rx(jumbo_budget, 1);
idm_net_rx(normal_budget, 0);
```

The callback calls the optional zero-argument `idm_recv_cmpl` hook after every
loop exit, including zero-count and exhausted-budget paths. With positive
remaining budget it completes NAPI slot 1, calls `idm_int_enable` through
`cpu_net_ops + 0x8` using `cpu_net_info_word_4`, and returns processed work.
Otherwise it returns the original budget and leaves NAPI scheduled.

## Relationship to IRQ Routing

`idm_wifi_int` masks `idm_info.word_4`, then calls `cpu_net_int(1)`, which
selects slot 1. This callback processes IDM RX queue 16 or 17 through
`idm_net_rx` depending on the jumbo selector and restores the same source word
only after NAPI completion.

## Concurrency

The hard IRQ owns initial source masking. The count read is an unlocked,
two-register hardware snapshot through `idm_get_cpu_rx_cnt`. The raw poll
counter is incremented with ordinary load/add/store instructions. No local lock
protects the external completion hook.

## Evidence

- Complete 78-instruction ARM64 disassembly at `0xc294` through `0xc3c8`.
- `cpu_net_init @ 0xe220` registers it at `int_info + 0x1a0` on the IDM netdev.
- `cpu_register_netinfo @ 0xe1ec` maps input word `+0x4` to `0x27dd8`.
- `idm_wifi_int @ 0x13b88` supplies source index 1 after masking word 4.
- Direct `idm_net_rx @ 0xbf6c` shows its selector maps to RX queue 16 or 17.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Registration, ownership, and intended synchronization contract of
  `idm_recv_cmpl` remain unresolved.
- The reason queue 8 is handled separately from CPU-net poll queues 0 through 7
  needs companion-module or runtime evidence.
