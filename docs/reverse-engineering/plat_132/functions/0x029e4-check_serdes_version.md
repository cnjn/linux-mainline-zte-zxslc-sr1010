# 0x029e4 check_serdes_version

## Status

- Status: complete
- Confidence: verified both MMIO reads and their order, fields, constants,
  branch precedence, exact logs, return behavior, and absence of direct xrefs.
- Size: `0x74` bytes, 25 ARM64 instructions.
- Recovered signature: `int check_serdes_version(void)`.

## Semantics

Reads SerDes offsets `0x4` and `0x18` before making any decision. It classifies
the version as follows:

| Condition | Log |
| --- | --- |
| offset `0x4` bits 1-4 equal 1 | V1 |
| offset `0x4` bits 1-4 are nonzero but not 1 | error |
| selector is 0 and offset `0x18` bits 16-31 equal `0x0ef0` | V2 |
| selector is 0 and offset `0x18` bits 16-31 equal `0x00ff` | V3 |
| otherwise | error |

Every path returns its `printk` result. The function does not return a version
number or a boolean success value.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be an
externally consumed diagnostic API.

## Evidence

- Complete ARM64 body at `0x29e4` through `0x2a54`.
- Up-front loads from offsets `0x4` and `0x18`.
- `UBFX #1,#4`, then comparisons against 1, `0x0ef0`, and `0x00ff` in the
  exact binary precedence.
- IDA type at `0x29e4` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
