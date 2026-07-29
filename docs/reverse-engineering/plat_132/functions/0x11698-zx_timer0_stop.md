# 0x11698 zx_timer0_stop

## Status

- Status: complete
- Confidence: verified wrapper behavior and semantic void ABI.
- Size: `0x14` bytes, 5 ARM64 instructions.
- Recovered signature: `void zx_timer0_stop(void)`.

## Semantics

Calls `timer0_stop` and has no other behavior. No direct module caller is
present; the module exports this entry through its ksymtab.

## Source-Like Reconstruction

`recovered/plat_timer.c`.
