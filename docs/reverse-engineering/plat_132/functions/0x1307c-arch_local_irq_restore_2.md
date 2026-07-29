# 0x1307c arch_local_irq_restore_2

## Status

- Status: complete
- Confidence: verified DAIF write and both direct callers; semantic void ABI.
- Size: `0xc` bytes, 3 ARM64 instructions.
- Recovered signature: `void arch_local_irq_restore_2(unsigned long flags)`.

## Semantics

Writes the supplied complete flags value to DAIF. The input remains as a raw
machine residual, but both IDM interrupt-mask callers ignore it.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
