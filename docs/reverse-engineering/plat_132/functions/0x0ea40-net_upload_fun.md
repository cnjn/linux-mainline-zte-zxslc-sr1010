# 0x0ea40 net_upload_fun

## Status

- Status: complete
- Confidence: verified debug/lock/reference-count/transition behavior, direct
  caller and hook publication; context/slowpath labels are strong inference.
- Size: `0x110` bytes, 68 ARM64 instructions.
- Recovered signature: `void net_upload_fun(int value)`.

## Semantics

This function is the upload-mode reference-count gate. If `net_gso_debug > 0`,

For nonzero input, it calls `gso_upload_enable` only on a zero-to-one transition,

The low-level function returns whatever the BH-enable helper leaves in the
return register, but the known direct caller ignores it; semantic source-like
return type is void.

## Caller and Hook Context

- `upload_write_proc @ 0x0eb50` parses a nonempty proc write as an unsigned
  decimal byte and calls this function, ignoring its result.
- `net_gso_init @ 0x0f9bc` stores this function into `upload_hook` even when
  proc creation fails.

## Concurrency and Ownership

- Updates `upload_count` under `net_lock_tx` with an acquire-exclusive fast
  path and queued-spinlock slowpath.
- No allocation, skb ownership, or direct MMIO occurs here.

## Evidence

- Complete 68-instruction ARM64 disassembly at `0xea40` through `0xeb4c`.
- Direct proc-write caller decompilation and initialization hook publication.
- Raw lock/barrier/global transition instruction sequence.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Exact `net_lock_slowpath_context` ABI and external upload-hook callers.
- Side effects/ownership within `gso_upload_enable` and `gso_upload_disable`.
