# 0x1314c idm_set_smct_all_trap

## Status

- Status: complete
- Confidence: verified input-bit extraction, volatile RMW, zero return, and
  exported-interface status.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature: `int idm_set_smct_all_trap(u32 enable)`.

## Semantics

Replaces IDM base register bit 14 with `enable & 1`, using one volatile 32-bit
read and one volatile 32-bit write to `nppt_base + 0x280000`. It always returns
zero and does not issue a barrier or take a lock.

## Caller Context

No direct module code caller exists; `__ksymtab_idm_set_smct_all_trap @
0x1c7ec` exports the control API.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- The exact hardware meaning of the SMCT all-trap enable bit is not established.
