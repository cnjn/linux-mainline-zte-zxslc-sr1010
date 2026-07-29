# 0x16c84 xmac_get_duplex_mode

## Status

- Status: complete
- Confidence: verified byte selector handling, both register-address formulas,
  bit-24 normalization, void return, and sole caller.
- Size: `0x58` bytes, 22 ARM64 instructions.
- Recovered signature: `void xmac_get_duplex_mode(u8 xmac, u32 *duplex)`.

## Semantics

The function selects one volatile 32-bit register from the byte-truncated XMAC
selector:

- XMAC `2` or `3`: raw address `((xmac + 7) << 16) + 0x140`.
- All other selectors: `nppt_base + (xmac << 18) + 0x140500`.

It reads bit 24 of that register and writes a normalized output: bit set yields
zero; bit clear yields one. It does not validate either selector or output
pointer and has no meaningful return value.

## Caller Context

The sole direct caller is `xmac_config_speed_duplex @ 0x18130` at `0x18180`,
where the normalized output selects its speed-only fast path versus full XMAC
reconfiguration.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It performs one
volatile read and writes caller-owned output storage.

## Evidence

- Complete 22-instruction ARM64 body at `0x16c84` through `0x16cd8`.
- Exact `(xmac - 2) <= 1` byte condition and special/NPPT-relative address
  calculations.
- Exact `TBZ` test of bit 24 and zero/one output stores.
- One direct caller xref, which consumes the output pointer rather than return
  register contents.
- IDA function type updated at `0x16c84` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware identity and polarity of the bit-24 duplex indication.
- Why XMAC selectors two/three use raw address windows.
