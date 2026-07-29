# 0x11a10 soam_init

## Status

- Status: complete
- Confidence: verified raw RMW, unbounded polling, log, and constant return.
- Size: `0x40` bytes, 16 ARM64 instructions.
- Recovered signature: `int soam_init(void)`.

## Semantics

RMW-sets bits zero and one in `nppt_base + 0x2c0000`, then repeatedly reads
`nppt_base + 0x80` until bit one becomes set. It performs no null check, timeout,
delay, relaxation instruction, lock, or error path. It logs `soam init done!`
and returns literal zero.

## Evidence

Complete body at `0x11a10` through `0x11a4c`; the same NPPT bank is used by the
adjacent NPPU reset logic. `0x11a0c` immediately before this function is
alternative-instruction data, not a control-flow predecessor.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.

## Open Questions

- Hardware meanings of the two written bits and ready bit remain unknown.
