# 0x13064 arch_local_irq_save_1

## Status

- Status: complete
- Confidence: verified DAIF snapshot/mask behavior and both direct callers.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `unsigned long arch_local_irq_save_1(void)`.

## Semantics

Reads and returns full DAIF, masking local IRQs with `MSR DAIFSet,#2` only if
bit seven was clear. `idm_int_enable` and `idm_int_disable` pair it with the
separate restore clone while updating the IDM interrupt mask under raw lock.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
