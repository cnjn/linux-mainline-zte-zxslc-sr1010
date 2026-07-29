# 0x11614 timer0_config

## Status

- Status: complete
- Confidence: verified guards, divisor, write order, and refresh call; semantic
  void ABI is strong inference.
- Size: `0x50` bytes, 20 ARM64 instructions.
- Recovered signature: `void timer0_config(u32 hz)`.

## Semantics

Returns without access when `hz == 0` or `g_timer0_base_2544 == NULL`. Otherwise
writes `25000000 / hz` to Timer0 `+0x08`, writes literal two to `+0x04`, then
calls the `+0x10` XOR-refresh helper. Integer division truncates normally.

## Evidence

Complete body at `0x11614` through `0x11660`; no direct code caller is present.

## Source-Like Reconstruction

`recovered/plat_timer.c`.
