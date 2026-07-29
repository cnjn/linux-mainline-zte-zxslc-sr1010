# 0x15e94 idm_exit

## Status

- Status: complete
- Confidence: verified.
- Size: `0x4` bytes, one ARM64 instruction.
- Recovered signature: `void idm_exit(void)`.

## Semantics

Empty cleanup hook containing only `RET`. It performs no resource release,
global mutation, lock operation, MMIO access, or diagnostic output.

## Caller Context

Its only direct module caller is `nppt_exit @ 0x11bcc`, at `0x11bd4`.

## Evidence

- Complete one-instruction body at `0x15e94`.
- Direct code xref at `0x11bd4`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
