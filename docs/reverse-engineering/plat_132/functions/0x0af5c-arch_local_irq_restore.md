# 0x0af5c arch_local_irq_restore

## Status

- Status: complete
- Confidence: verified full-system-register write, raw residual return register,
  and all direct callers.
- Size: `0xc` bytes, 3 ARM64 instructions.
- Recovered signature: `void arch_local_irq_restore(unsigned long flags)`.

## Semantics

Writes the supplied 64-bit value directly to the complete `DAIF` system
register with `MSR DAIF, X0`, then returns. It therefore restores every DAIF
mask bit represented in the saved value, not only the IRQ bit altered by
`__raw_spin_lock_irqsave`.

`x0` remains unchanged through the instruction sequence, so the machine return
register contains the input flags value. All three direct callers ignore it;
the semantic return type is void.

## Caller Context

The three direct call sites are TX unlock paths in `idm_net_tx @ 0xd234` and
`cpu_net_tx @ 0xd668`. Each releases its raw lock with a byte-width store-release
before invoking this helper with the DAIF value saved by
`__raw_spin_lock_irqsave`.

## Globals and Concurrency

No module globals, MMIO, allocation, callback, or ownership behavior. It
changes CPU-local interrupt/debug/SError/FIQ mask state according to the caller
supplied snapshot.

## Evidence

- Complete three-instruction ARM64 body at `0xaf5c` through `0xaf64`.
- Exact `MSR DAIF, X0`, followed by `NOP` and `RET`.
- Three direct TX caller xrefs and the paired irqsave reconstruction.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.

## Open Questions

- Whether any indirect/external call site relies on the residual return register.
