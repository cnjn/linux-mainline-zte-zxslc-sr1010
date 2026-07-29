# 0x11664 timer0_start

## Status

- Status: complete
- Confidence: verified.
- Size: `0x1c` bytes, 7 ARM64 instructions.
- Recovered signature: `void timer0_start(void)`.

## Semantics

When Timer0 has been mapped, writes one to `g_timer0_base_2544 + 0x0c`; otherwise
returns without side effects. Its sole direct caller is `zx_timer0_start`.

## Source-Like Reconstruction

`recovered/plat_timer.c`.
