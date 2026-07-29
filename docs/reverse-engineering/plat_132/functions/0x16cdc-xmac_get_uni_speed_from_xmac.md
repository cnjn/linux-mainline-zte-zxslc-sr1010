# 0x16cdc xmac_get_uni_speed_from_xmac

## Status

- Status: complete
- Confidence: verified selector handling, speed-select field read, all eight
  computed-dispatch targets, output write, void return, and sole caller.
- Size: `0x54` bytes, 20 ARM64 instructions plus eight tail dispatch stubs.
- Recovered signature:
  `void xmac_get_uni_speed_from_xmac(u8 xmac, int *speed)`.

## Semantics

The function selects a volatile speed-control word using the byte selector:

- XMAC `2` or `3`: `(xmac + 7) << 16`.
- All other selectors: `nppt_base + (xmac << 18) + 0x140000`.

It extracts bits `31:29` and writes the mapped UNI speed to caller storage:

| Speed-select field | UNI speed |
| --- | --- |
| `0` | `6` |
| `1` | `7` |
| `2` | `4` |
| `3` | `3` |
| `4` | `2` |
| `5` | `5` |
| `6` | `4` |
| `7` | `1` |

The binary implements the table as a byte-indexed computed branch into eight
tiny `MOV`/store/return stubs. The reconstruction expresses the same total map
as a C table.

## Caller Context

The sole direct caller is `phy_zxic051_check @ 0x1c0c0` at `0x1c2cc`, immediately
after `xmac_speed_process`; it compares the output against converted PHY speed.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. It performs one
volatile read and writes caller-owned output storage.

## Evidence

- Complete 20-instruction dispatcher at `0x16cdc` through `0x16d2c`.
- Exact selector address formulas and `LSR #29` extraction.
- Branch-offset bytes at `0x1e608` and all eight tail stubs at `0x16d30` through
  `0x16d68`, yielding the recorded mapping.
- One direct caller xref and caller assembly showing the output pointer at `X1`.
- IDA function type updated at `0x16cdc` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of duplicated mapping value four and the nonmonotonic codes.
