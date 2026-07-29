# 0x11b3c nppt_tm_reset

## Status

- Status: complete
- Confidence: verified register reads/writes, bit, delay, log order, and zero
  return.
- Size: `0x90` bytes, 32 ARM64 instructions.
- Recovered signature: `int nppt_tm_reset(void)`.

## Semantics

Matches `nppt_nppu_reset` exactly except it pulses bit eight of
`nppt_base + 0x2c0004`: clear/write/log, delay `1718000`, set/write/log, then
return zero. No direct IDA caller is present.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
