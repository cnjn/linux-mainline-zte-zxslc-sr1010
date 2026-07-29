# 0x11528 timer_refresh_config_load_reg

## Status

- Status: complete
- Confidence: verified raw read/XOR/write transaction and callers; semantic void
  ABI is strong inference because all callers ignore the residual word.
- Size: `0x24` bytes, 9 ARM64 instructions.
- Recovered signature: `void timer_refresh_config_load_reg(void __iomem *base)`.

## Semantics

Reads timer word `base + 0x10`, XORs it with `0x0f`, and writes it back to the
same offset. No null check, delay, barrier, or status handling occurs locally.

## Caller Context and Evidence

Called after Timer0 ordinary/dothz reload programming and Timer1 setup. The
complete body is at `0x11528` through `0x11548`; its only helper is
`__raw_readl`.

## Source-Like Reconstruction

`recovered/plat_timer.c`.
