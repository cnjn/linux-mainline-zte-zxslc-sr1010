# 0x1305c __my_cpu_offset_0

## Status

- Status: complete
- Confidence: verified raw system-register read and eleven direct callers.
- Size: `0x8` bytes, 2 ARM64 instructions.
- Recovered signature: `uintptr_t __my_cpu_offset_0(void)`.

## Semantics

Reads `TPIDR_EL1` and returns it unchanged. IDM callers add it to static
per-CPU state bases; no state is modified locally.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
