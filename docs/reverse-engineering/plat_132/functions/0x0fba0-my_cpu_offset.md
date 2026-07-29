# 0x0fba0 __my_cpu_offset

## Status

- Status: complete
- Confidence: verified system-register read, return value, and all nine direct
  callers; per-CPU-offset naming is supported by caller address arithmetic.
- Size: `0x8` bytes, 2 ARM64 instructions.
- Recovered signature: `uintptr_t __my_cpu_offset(void)`.

## Semantics

Reads `TPIDR_EL1` into the return register and immediately returns. It takes no
arguments and has no writes, lock, allocation, callback, MMIO, or ownership
behavior.

The function's name and caller use strongly support interpreting the value as a
CPU-local offset/base: callers add it to static per-CPU state regions before
accessing FIFO, skb-stack, or allocation state. The binary itself establishes
only the raw `TPIDR_EL1` read.

## Caller Context

Nine direct call sites occur in `_idm_skb_stack_push`, `idm_skb_stack_push`,
`idm_skb_stack_pop`, `net_alloc_skb`, `net_alloc_kmem`, `net_free_kmem`, and
`buf_fifo_rls`. Each consumes the returned value as part of CPU-local address
calculation.

## Evidence

- Complete two-instruction ARM64 body at `0xfba0` through `0xfba4`.
- Exact `MRS X0, TPIDR_EL1` followed by `RET`.
- Nine direct caller xrefs with per-CPU base-address arithmetic.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Exact vendor per-CPU data layout and whether TPIDR_EL1 is always an offset
  rather than a direct base pointer in this kernel configuration.
