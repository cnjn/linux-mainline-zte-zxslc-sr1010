# 0x0fb7c arch_local_irq_save

## Status

- Status: complete
- Confidence: verified DAIF read, conditional IRQ masking, return value, and
  both direct FIFO callers.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `unsigned long arch_local_irq_save(void)`.

## Semantics

Reads and returns the complete `DAIF` system-register value. If DAIF bit 7 is
clear, it executes `MSR DAIFSet, #2` to mask local IRQs. If the bit is already
set, it returns without writing DAIF. It does not acquire a lock, access module
state, allocate, or perform a later restore itself.

## Caller Context

`buf_fifo_free_data @ 0xfc64` and `buf_fifo_alloc_data @ 0x1003c` each use the
saved flags while manipulating per-CPU FIFO state. Their paired
`arch_local_irq_restore_0 @ 0xfb94` calls occur after the critical operations.

## Globals and Concurrency

Only CPU-local interrupt masking changes. FIFO callers own their data-structure
locking and restore the captured state themselves.

## Evidence

- Complete six-instruction ARM64 body at `0xfb7c` through `0xfb90`.
- Exact `MRS DAIF`, bit-7 test, optional `MSR DAIFSet, #2`, and return.
- Two direct caller xrefs in the FIFO allocation/free helpers.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- FIFO critical-section rules and whether their callers can run in all DAIF
  states.
