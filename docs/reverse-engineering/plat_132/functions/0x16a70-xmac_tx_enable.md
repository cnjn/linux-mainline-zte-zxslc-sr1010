# 0x16a70 xmac_tx_enable

## Status

- Status: complete
- Confidence: verified raw selector tests/masks, both register formulas,
  bit-zero RMW, void return, and sole caller.
- Size: `0x64` bytes, 25 ARM64 instructions.
- Recovered signature: `void xmac_tx_enable(u32 xmac)`.

## Semantics

The helper chooses a volatile TX control word without truncating the selector to

- Raw selector `2` or `3`: `((u16)(xmac + 7)) << 16`.
- All other values: `nppt_base + ((xmac & 0x3fff) << 18) + 0x140000`.

It reads the word, ORs bit zero, and writes it back. No selector validation,
lock, error path, or meaningful return value exists.

## Caller Context

The sole direct caller is `xmac_tx_rx_enable @ 0x16bbc` at `0x16bd0`, after RX
enable has run with the same selector.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. This is a volatile
MMIO-style read-modify-write; caller serialization controls concurrent access.

## Evidence

- Complete 25-instruction ARM64 body at `0x16a70` through `0x16ad0`.
- Exact unsigned raw `xmac - 2 <= 1` special selector condition.
- Exact 16-bit special and 14-bit NPPT-relative selector masks plus bit-zero
  read-modify-write.
- One direct caller xref, which discards the return register.
- IDA function type updated at `0x16a70` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of the selector widths and TX control bit zero.
