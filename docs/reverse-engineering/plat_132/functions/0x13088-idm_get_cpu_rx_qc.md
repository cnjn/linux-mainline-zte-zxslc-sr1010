# 0x13088 idm_get_cpu_rx_qc

## Status

- Status: complete
- Confidence: verified.
- Size: `0x14` bytes, 5 ARM64 instructions.
- Recovered signature:
  `struct idm_rx_queue *idm_get_cpu_rx_qc(u32 index)`.

## Semantics

The function has no bounds check and returns a 16-byte-stride entry in the
global RX queue state array:

```c
return &idm_rx_q[index];
```

The exact ARM64 address calculation is `idm_rx_q + 16 * index`. This corroborates
the 16-byte RX queue state layout used in the `idm_init` reconstruction.

## Call Context

- It has no direct code callers in this module.
- The only xref is an IDM ops-table entry at `0x266f0`, so external consumers
  can pass arbitrary indices. The lack of bounds checking is therefore part of
  the exported callback contract.

## Evidence

- Full five-instruction ARM64 disassembly at `0x13088` through `0x13098`.
- Raw IDM ops-table data at `0x266d8`.
- RX descriptor initialization in `idm_init @ 0x14ff4` uses 24 consecutive
  16-byte entries between `idm_rx_q` and `idm_tx_q`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- The source-level name and full meaning of the three RX queue fields remain
  inferred from layout and initializer behavior.
- External callers must establish index validity; none is enforced here.
