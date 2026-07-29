# 0x11680 timer0_stop

## Status

- Status: complete
- Confidence: verified.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `void timer0_stop(void)`.

## Semantics

When Timer0 has been mapped, writes zero to `g_timer0_base_2544 + 0x0c`; otherwise
returns without side effects. Called by `zx_timer0_start` and `zx_timer0_stop`.

## Source-Like Reconstruction

`recovered/plat_timer.c`.
