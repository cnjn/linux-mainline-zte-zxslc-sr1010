# 0x11bfc arch_local_irq_restore_1

## Status

- Status: complete
- Confidence: verified DAIF write and both direct callers; semantic void ABI is
  established by ignored input-valued residual return.
- Size: `0xc` bytes, 3 ARM64 instructions.
- Recovered signature: `void arch_local_irq_restore_1(unsigned long flags)`.

## Semantics

Writes the supplied full flags value to DAIF with `MSR DAIF,X0`, then returns.
It is the paired restore helper for the two GREG auto-gate accessors and remains
a separately represented binary entry.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
