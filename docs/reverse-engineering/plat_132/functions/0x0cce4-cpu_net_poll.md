# 0x0cce4 cpu_net_poll

## Status

- Status: complete
- Confidence: verified control flow, loop bounds, packed-count interpretation,
  ops-table slots, source-mask bits, NAPI completion condition, and return
  behavior. `cpu_net_rx` payload processing is intentionally delegated to its
  separate reconstruction.
- Size: `0x1a8` bytes, 106 ARM64 instructions.
- Recovered signature: `int cpu_net_poll(struct napi_struct *napi, int budget)`.

## Semantics

This is the NAPI poll callback attached to `int_info`; it ignores its `napi`
argument and always completes the global `int_info` context. It increments the
raw global at `0x27c44`, then scans CPU RX queues 7 down through 0 for up to
four passes.

For each enabled scan slot it calls `cpu_net_ops + 0x10`
(`idm_get_cpu_rx_cnt(queue)`). The returned word is consumed as:

```text
bits  0..15: normal-buffer available count
bits 16..31: jumbo-buffer available count
```

Each class is capped at `remaining_budget >> 1`, using the same pre-dispatch
budget for both caps. Jumbo work calls `cpu_net_rx(count, queue, 1)` only if
bit `queue + 8` of `cpu_net_info_word_0` is set; normal work calls
`cpu_net_rx(count, queue, 0)` only if bit `queue` is set. Returned work counts
are subtracted from the remaining budget and added to the processed total.

Empty queues 0 through 6 are removed from the persistent scan mask. Queue 7 is
never removed, so it is rechecked on every pass. A nonzero packed count makes a
pass eligible for another iteration even if both class masks deny dispatch or
the half-budget is zero. Consequently a budget of one can perform no RX work
and still complete NAPI after the four-pass bound.

`pp_tcp_gro_flush_all()` always runs after the scan. If budget remains positive,
the function calls `napi_complete(&int_info)`, invokes `cpu_net_ops + 0x8`
(`idm_int_enable`) with `cpu_net_info_word_0`, and returns the processed count.
If the budget is exhausted, it leaves NAPI scheduled and returns the original
budget.

## Concurrency

The preceding hard IRQ masks `cpu_net_info_word_0` before calling
`cpu_net_int(0)`. This poll path restores that source only after it decides
there is unused budget. Queue-count reads originate in IDM hardware and use no
local lock or snapshot retry; `idm_get_cpu_rx_cnt` itself combines two register
reads. The poll-call counter at `0x27c44` is incremented with an ordinary
load/add/store sequence.

## Evidence

- Complete 106-instruction ARM64 disassembly at `0xcce4` through `0xce88`.
- `cpu_net_init @ 0xe220` installs this callback for `int_info` with NAPI
  weight 512.
- `cpu_net_ops + 0x10` is `idm_get_cpu_rx_cnt`; `+0x8` is `idm_int_enable`.
- `cpu_net_info_word_0` xrefs connect the same word to open, stop, registration,
  and this poll's class masks.
- Direct `cpu_net_rx @ 0xc5dc` call instructions preserve argument order:
  count in `w0`, queue in `w1`, jumbo selector in `w2`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Original names and intended consumers of raw poll counter `0x27c44` and the
  two class-enable bytes in `cpu_net_info_word_0` remain unknown.
- The special persistence of queue 7 and the integer half-budget policy need
  vendor-source or runtime-load evidence to explain their design intent.
