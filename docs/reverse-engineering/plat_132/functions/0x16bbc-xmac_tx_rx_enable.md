# 0x16bbc xmac_tx_rx_enable

## Status

- Status: complete
- Confidence: verified preserved raw selector flow, RX-then-TX call order, void
  return, and all three direct callers.
- Size: `0x20` bytes, 8 ARM64 instructions.
- Recovered signature: `void xmac_tx_rx_enable(u32 xmac)`.

## Semantics

The function preserves the complete 32-bit input selector across a call to
`xmac_rx_enable`, then calls `xmac_tx_enable` with the same selector. It has no
local register access, error path, or meaningful return value.

## Caller Context

Direct calls occur in:

- `nppt_smac_enable @ 0x12250` for delegated XMAC link-up enable.
- `xmac_test_siwtch_work_mode @ 0x17a50`.
- `xmac_init_by_work_mode @ 0x17da0` after mode setup.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. RX/TX enable effects
are delegated in the observed RX-first order.

## Evidence

- Complete 8-instruction ARM64 body at `0x16bbc` through `0x16bd8`.
- Raw `W0` copied to `W4` without truncation, RX enable call, selector restore,
  then TX enable call.
- Three direct caller xrefs, all treating it as a void operation.
- IDA function type updated at `0x16bbc` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware behavior if RX enable succeeds but TX enable fails or stalls.
