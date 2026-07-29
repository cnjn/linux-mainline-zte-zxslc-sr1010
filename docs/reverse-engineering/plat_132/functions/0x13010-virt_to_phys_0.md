# 0x13010 virt_to_phys_0

## Status

- Status: complete
- Confidence: verified predicate, imported values, both arithmetic paths, and
  five direct callers.
- Size: `0x4c` bytes, 19 ARM64 instructions.
- Recovered signature: `u64 virt_to_phys_0(const void *address)`.

## Semantics

Uses the same local conversion formula as `virt_to_phys @ 0xe5e8`: it computes
a `vabits_actual`-dependent limit, then returns either
`address - kimage_voffset` or `(address & 0x7fffffffff) + memstart_addr`.
`memstart_addr`, `kimage_voffset`, and `vabits_actual` are 64-bit imported
values. No range, page, DMA, or cache validation occurs.

## Caller Context

IDM refill and all three IDM TX backends use this entry for descriptor physical
addresses; `_idm_rx_refill` is also a direct caller.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
