# 0x11be4 arch_local_irq_save_0

## Status

- Status: complete
- Confidence: verified DAIF read, conditional IRQ masking, return value, and
  both direct callers.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `unsigned long arch_local_irq_save_0(void)`.

## Semantics

Reads and returns complete DAIF. When bit seven is clear, executes `MSR
DAIFSet,#2` to mask local IRQs; otherwise writes no DAIF state. This is a
distinct binary helper from the other recovered save entries.

## Caller Context

`greg_sopc_auto_gate_en_get` and `greg_sopc_auto_gate_en_set` use it around
their CRM accesses.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
