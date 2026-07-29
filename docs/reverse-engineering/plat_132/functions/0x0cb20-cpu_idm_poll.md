# 0x0cb20 cpu_idm_poll

## Status

- Status: complete
- Confidence: verified loop/control flow, NAPI slot, source word, packed-count
  interpretation, ops slots, class masks, and return behavior.
- Size: `0x1c4` bytes, 113 ARM64 instructions.
- Recovered signature: `int cpu_idm_poll(struct napi_struct *napi, int budget)`.

## Semantics

This callback is installed on NAPI slot 2 (`int_info + 0x340`) and services the
source represented by `cpu_net_info_word_8`. It ignores its passed `napi`
argument, increments the raw poll counter at `0x27f84`, and makes up to four
high-to-low scans of queue indices 7 through 0.

Unlike `cpu_net_poll`, a queue is considered only if its normal or jumbo class
bit is enabled in `cpu_net_info_word_8`. The callback obtains a packed count
from `cpu_net_ops + 0x10` (`idm_get_cpu_rx_cnt`), interprets low 16 bits as
normal and high 16 bits as jumbo count, caps each class to half the current
remaining budget, and calls `cpu_net_rx(count, queue, jumbo_selector)` for each
enabled class.

Empty eligible queues 0 through 6 are removed from the scan mask; queue 7 stays
eligible across all passes. If the remaining budget is exhausted before a queue
count read, the function immediately returns the original budget. It does not
flush GRO on that path or on ordinary completion.

After the scan, positive remaining budget causes
`napi_complete(int_info + 0x340)` followed by `cpu_net_ops + 0x8`
(`idm_int_enable(cpu_net_info_word_8)`). It returns processed work only on that
completion path; otherwise it returns the original budget and leaves NAPI
scheduled.

## Relationship to IRQ Routing

`idm_all_int` masks `idm_info.word_8` and calls `cpu_net_int(2)`. The latter
selects NAPI slot 2, so this poll is the localtest/CPU-IDM deferred path. The
slot must not be confused with source 1, which is slot 1 and runs
`idm_net_poll`.

## Concurrency

The hard IRQ masks the source before scheduling. This function re-enables it
only after NAPI completion. Queue counts come from the two-register
`idm_get_cpu_rx_cnt` snapshot and the raw poll counter increment has no local
atomic operation or lock.

## Evidence

- Complete 113-instruction ARM64 disassembly at `0xcb20` through `0xcce0`.
- `cpu_net_init @ 0xe220` registers this callback at `int_info + 0x340`.
- `cpu_register_netinfo @ 0xe1ec` maps input word `+0x8` to `0x27f78`.
- `idm_all_int @ 0x13b38` supplies source index 2 after masking word 8.
- Direct RX call setup shows count/queue/jumbo arguments in `w0`/`w1`/`w2`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- The distinction between source-0's unconditional queue scan/GRO flush and
  this source's enabled-only/no-flush policy requires vendor semantics.
- Original names and consumers of the raw counter at `0x27f84` are unknown.
