# 0x11734 zx_timer_wclk_sel

## Status

- Status: complete
- Confidence: verified guards and both ordered RMWs; clock-source labels beyond
  the DT mux evidence remain inference.
- Size: `0x60` bytes, 24 ARM64 instructions.
- Recovered signature: `void zx_timer_wclk_sel(u32 select)`.

## Semantics

When `g_lsp0_base` is mapped, derives `(select & 1) << 9` and independently
replaces bit nine in LSP0 CRM words `+0x04` and `+0x08`; null base is a no-op.
Timer initialization calls it with one. Runtime DT clock-parent order supports
but does not prove that this selects the 25 MHz parent.

## Source-Like Reconstruction

`recovered/plat_timer.c`.
