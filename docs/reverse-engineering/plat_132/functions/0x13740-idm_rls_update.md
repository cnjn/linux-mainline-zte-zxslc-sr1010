# 0x13740 idm_rls_update

## Status

- Status: complete
- Confidence: verified CPU-family gates, repeated probes, packed/direct MMIO
  writes, and ops-table role.
- Size: `0x84` bytes, 33 ARM64 instructions.
- Recovered signature:
  `void idm_rls_update(u32 count_0, u32 count_1, u32 count_2)`.

## Semantics

Only on CPU type 133 or 129, writes `count_0 | (count_1 << 16)` to IDM `+0x7c`.
It then repeats the same CPU-type probe and, if still accepted, writes `count_2`

## Caller Context

The only xref is the IDM ops-table slot at `0x26738` (`+0x60`); recovered TX
reclamation passes three release counts through that slot.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
