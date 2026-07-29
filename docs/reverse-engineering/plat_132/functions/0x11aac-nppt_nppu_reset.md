# 0x11aac nppt_nppu_reset

## Status

- Status: complete
- Confidence: verified register reads/writes, bit, delay, log order, and zero
  return.
- Size: `0x90` bytes, 32 ARM64 instructions.
- Recovered signature: `int nppt_nppu_reset(void)`.

## Semantics

Reads `nppt_base + 0x2c0004`, logs the raw word/address, clears bit seven and
writes it, logs that reset value, delays with `__const_udelay(1718000)`, then
sets bit seven in the reset value, writes it, logs the restored word, and returns
zero. It does not preserve an independently reread post-delay value.

No direct IDA caller is present; runtime kallsyms marks it module-local.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
