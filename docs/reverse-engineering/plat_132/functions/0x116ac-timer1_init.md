# 0x116ac timer1_init

## Status

- Status: complete
- Confidence: verified guards, ordered MMIO, barrier, and semantic void ABI.
- Size: `0x50` bytes, 20 ARM64 instructions.
- Recovered signature: `void timer1_init(void)`.

## Semantics

If `g_timer1_base` is null, returns. Otherwise writes `0xffffffff` to `+0x08`,

## Source-Like Reconstruction

`recovered/plat_timer.c`.
