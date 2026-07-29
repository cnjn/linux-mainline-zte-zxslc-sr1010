# 0x10808 __raw_spin_lock_bh.constprop.13

## Status

- Status: complete
- Confidence: verified BH/preempt word mutation, lock fast path, slowpath
  arguments, callers, and return-use; raw context-field label is inferred.
- Size: `0x5c` bytes, 23 ARM64 instructions.
- Recovered signature: `void __raw_spin_lock_bh_constprop_13(void)`.

## Semantics

The function reads `SP_EL0`, increments its 32-bit field at offset `+0x10` by
`0x200`, then acquires the global 32-bit `groport_busy_lock`.

Its fast path prefetches the lock, repeatedly performs acquire exclusive loads,
nonzero observed value calls:

```c
queued_spin_lock_slowpath(&groport_busy_lock, observed, 0, 1);
```

The apparent return pointer to `net_smb_state` is residual register state; all
callers use it as a void lock operation. `is_l4port_supported` releases the low
lock byte with store-release, then restores BH state via
`__local_bh_enable_ip`.

## Callers

- `add_supported_l4port @ 0x10864`
- `is_l4port_supported @ 0x10930`
- `remove_supported_l4port @ 0x109d0`

## Concurrency and Ownership

- This is the synchronization boundary for GRO configured port lists.
- It mutates current CPU-local state before attempting the lock.
- No allocation, callback, direct MMIO, or ownership transfer occurs.

## Evidence

- Complete 23-instruction ARM64 disassembly at `0x10808` through `0x10860`.
- Three direct caller xrefs and matching caller-side byte-width release.
- Raw exclusive-load/store loop and slowpath register setup.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Exact current-context field at `SP_EL0 + 0x10` and vendor kernel locking ABI.
- Why the caller releases only the low lock byte rather than invoking a common
  unlock helper.
