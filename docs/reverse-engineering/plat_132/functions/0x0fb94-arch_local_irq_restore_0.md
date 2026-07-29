# 0x0fb94 arch_local_irq_restore_0

## Status

- Status: complete
- Confidence: verified full-system-register write, raw residual return register,
  and all four direct FIFO callers.
- Size: `0xc` bytes, 3 ARM64 instructions.
- Recovered signature: `void arch_local_irq_restore_0(unsigned long flags)`.

## Semantics

Writes the supplied value directly to the complete `DAIF` register with
`MSR DAIF, X0`, then returns. The input remains in `x0`, leaving an
input-valued residual return register that all observed callers ignore. Its
semantic return type is void.

This is a distinct binary entry with behavior identical to
`arch_local_irq_restore @ 0xaf5c`; it must remain separately represented because
its direct callers belong to the FIFO allocation/free path.

## Caller Context

Four direct calls occur in `buf_fifo_free_data @ 0xfc64` and
`buf_fifo_alloc_data @ 0x1003c`. They restore flags captured by
`arch_local_irq_save @ 0xfb7c` after their FIFO critical-section operations.

## Globals and Concurrency

No module globals, MMIO, allocation, callback, or ownership behavior. It
changes CPU-local DAIF state exactly as supplied by the caller.

## Evidence

- Complete three-instruction ARM64 body at `0xfb94` through `0xfb9c`.
- Exact `MSR DAIF, X0`, `NOP`, and `RET` sequence.
- Four direct FIFO caller xrefs and paired save-helper reconstruction.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Whether an external/indirect caller relies on the residual return register.
