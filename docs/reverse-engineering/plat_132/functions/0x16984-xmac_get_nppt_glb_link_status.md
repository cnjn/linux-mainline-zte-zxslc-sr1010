# 0x16984 xmac_get_nppt_glb_link_status

## Status

- Status: complete
- Confidence: verified register offset, ARM variable-shift behavior, normalized
  output, void return, and sole caller.
- Size: `0x1c` bytes, 7 ARM64 instructions.
- Recovered signature:
  `void xmac_get_nppt_glb_link_status(u32 xmac, int *link_up)`.

## Semantics

The function reads the 32-bit word at `nppt_base + 0x84`, shifts it right by the
low five bits of raw `xmac`, masks bit zero, and writes normalized zero/one to
`*link_up`. It performs no selector or output-pointer validation and has no
meaningful return value.

## Caller Context

The sole direct caller is `phy_zxic051_check @ 0x1c0c0` at `0x1c218`, where the
result gates PHY/XMAC speed reconciliation and recovery counters.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It performs one
volatile status read and writes caller-owned output storage.

## Evidence

- Complete 7-instruction ARM64 body at `0x16984` through `0x1699c`.
- Exact word load at offset `0x84`, variable `LSR`, bit-zero mask, and output
  store.
- One direct caller xref from `phy_zxic051_check`.
- IDA function type updated at `0x16984` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware identity of the global-link status word and bit-to-XMAC mapping.
