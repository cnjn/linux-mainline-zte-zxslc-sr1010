# 0x116fc timer1_get_counter

## Status

- Status: complete
- Confidence: verified.
- Size: `0x2c` bytes, 11 ARM64 instructions.
- Recovered signature: `u32 timer1_get_counter(void)`.

## Semantics

Returns zero when Timer1 is unmapped; otherwise returns one raw 32-bit word from
`g_timer1_base + 0x18`. Counter direction and units remain unknown.

## Source-Like Reconstruction

`recovered/plat_timer.c`.
