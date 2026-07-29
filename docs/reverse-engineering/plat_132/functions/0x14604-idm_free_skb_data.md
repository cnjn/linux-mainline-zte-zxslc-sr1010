# 0x14604 idm_free_skb_data

## Status

- Status: complete
- Confidence: verified flag branches, raw skb offsets, bit-15 auxiliary loop,
  boundary classifications, cache/FIFO destinations, counters, module-init
  callback assignment, and void ABI.
- Size: `0x2b0` bytes, 171 ARM64 instructions.
- Recovered signature: `void idm_free_skb_data(struct zte_skb *skb)`.

## Semantics

Reads raw skb word `+0x114`. If bit zero is clear, it calls `kfree` on the
pointer at `+0x128` and returns. Otherwise it derives the primary pool from bit
one and returns the primary backing pointer through either the corresponding
FIFO or normal/jumbo slab cache according to a recomputed reserved-boundary
comparison. These paths update the corresponding `idm_status` lane.

When flag bit 15 is set, it first increments `dword_288A0` and walks a
byte-counted auxiliary table. The record begins at `skb[+0x128] + skb[+0x120]`;
its byte `+2` is the number of 32-bit raw buffer addresses. Each address is
converted through `memstart_addr`, adjusted by `uBP_BUFFER_OFFSET + 64`, then
sent to normal cache free or FIFO0. This loop completes before the primary
backing pointer path.

No raw field, count, pool bit, or table-derived address is validated. The
function is installed into `pp_free_skb_data` as a callback, not as a data
object.

## Caller Context

`idm_init @ 0x14ff4` stores this function address into `pp_free_skb_data`; no
direct BL caller exists in this module.

## Evidence

- Complete ARM64 body at `0x14604` through `0x148b0`.
- Bit-zero `kfree` branch at `0x14624` through `0x14894`.
- Bit-15 auxiliary table loop at `0x146fc` through `0x1484c`.
- Primary FIFO/cache branch at `0x14650` through `0x146f8` and
  `0x14854` through `0x14888`.
- Function-address xref from `idm_init` at `0x15e34`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- Exact source-level names/ownership meanings of skb offsets `+0x114`, `+0x118`,
  `+0x120`, and `+0x128`, plus flag bit 15, remain unresolved.
