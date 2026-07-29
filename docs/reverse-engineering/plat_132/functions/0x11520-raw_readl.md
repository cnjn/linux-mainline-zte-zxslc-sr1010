# 0x11520 __raw_readl

## Status

- Status: complete
- Confidence: verified.
- Size: `0x8` bytes, 2 ARM64 instructions.
- Recovered signature: `u32 __raw_readl(const volatile u32 *address)`.

## Semantics

Returns one 32-bit volatile word from the supplied address. It has no validation,
locking, allocation, callback, or ownership behavior.

The adjacent `0x11a0c` item is ARM64 `.altinstructions` data, not a function: it
can replace this entry's `LDR W0,[X0]` with `LDAR W0,[X0]` for feature 1. It must
not be reconstructed as a C fall-through into `soam_init`.

## Caller Context and Evidence

Four direct calls occur in Timer0/Timer1 configuration and clock-selection
paths. The complete body is `LDR W0,[X0]; RET` at `0x11520` through `0x11524`.

## Source-Like Reconstruction

`recovered/plat_timer.c`.
