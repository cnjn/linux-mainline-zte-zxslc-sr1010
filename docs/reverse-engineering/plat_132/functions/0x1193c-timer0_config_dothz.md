# 0x1193c timer0_config_dothz

## Status

- Status: complete
- Confidence: verified guards, divisor, write order, and caller.
- Size: `0x50` bytes, 20 ARM64 instructions.
- Recovered signature: `void timer0_config_dothz(u32 rate)`.

## Semantics

For nonzero rate and a mapped Timer0, writes `250000000 / rate` to `+0x08`,
used by `zx_timer0_start`; the meaning of the 250 MHz denominator is unresolved.

## Source-Like Reconstruction

`recovered/plat_timer.c`.
