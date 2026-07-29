# 0x130f8 idm_recv_debug_set

## Status

- Status: complete
- Confidence: verified no-op body and exported-interface status.
- Size: `0x4` bytes, 1 ARM64 instruction.
- Recovered signature: `void idm_recv_debug_set(void)`.

## Semantics

The entire body is `RET`; it reads and writes no architectural state. It is an
exported compatibility stub rather than a functional debug configuration path.

## Caller Context

There are no direct module code callers. `__ksymtab_idm_recv_debug_set @
0x1c7c8` exports the entry.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
