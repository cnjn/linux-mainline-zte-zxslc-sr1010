# 0x0b2a8 cpu_net_timeout

## Status

- Status: complete
- Confidence: verified direct instruction flow, imported queue-wake call, queue
  offset, timestamp offset, and both netdev-ops table references.
- Size: `0x44` bytes, 17 ARM64 instructions.
- Recovered signature: `void cpu_net_timeout(struct net_device *device)`.

## Semantics

The callback has no reset, logging, locking, descriptor, or MMIO work. It:

1. Loads the queue pointer from `device + 0x3c0`.
2. Calls `netif_tx_wake_queue(queue)`.
3. Reads `jiffies` and queue word `queue + 0x88`.
4. Stores a freshly re-read `jiffies` to `queue + 0x88` only when its old value
   differs.

IDA exposes the residual queue pointer in `x0` at return, but this is not a
semantic return value. The recovered callback is `void`, consistent with the
single-argument call shape and its placement in both netdev-ops tables.

## Evidence

- Complete 17-instruction ARM64 disassembly at `0xb2a8` through `0xb2e8`.
- Imported call to `netif_tx_wake_queue` at `0xb2bc`.
- `jiffies` reads at `0xb2c4`, `0xb2c8`, and `0xb2d8`.
- Function-pointer entries at `0x1dcf8` and `0x1df18`, in the CPU and IDM
  netdev-ops tables respectively.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- The exact kernel member name for the queue word at `+0x88` is inferred as
  `trans_start` from the `netif_tx_wake_queue` context, not vendor debug data.
- There is no evidence in this function for a higher-level watchdog recovery
  policy; any such policy must live in the networking core or another callback.
