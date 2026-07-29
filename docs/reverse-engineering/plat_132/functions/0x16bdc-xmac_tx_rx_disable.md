# 0x16bdc xmac_tx_rx_disable

## Status

- Status: complete
- Confidence: verified preserved raw selector flow, RX-then-TX call order, void
  return, and sole direct caller.
- Size: `0x20` bytes, 8 ARM64 instructions.
- Recovered signature: `void xmac_tx_rx_disable(u32 xmac)`.

## Semantics

The function preserves the full 32-bit input selector across a call to
`xmac_rx_disable`, then calls `xmac_tx_disable` with the same selector. It does
not truncate the selector, access registers locally, or provide an error path.

## Caller Context

The sole direct caller is `nppt_smac_disable @ 0x12178` at `0x121ec`, for an
out-of-range non-MAC6 selector after subtracting four from the SMAC value.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. RX/TX disable effects
are delegated in the observed RX-first order.

## Evidence

- Complete 8-instruction ARM64 body at `0x16bdc` through `0x16bf8`.
- Selector copied from `W0` to `W4`, RX disable call, selector restore, then TX
  disable call.
- One direct caller xref, which discards the return register.
- IDA function type updated at `0x16bdc` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware behavior on partial RX/TX disable.
