# 0x02fe4 serdes_set_pattern

## Status

- Status: complete
- Confidence: verified argument widths, 80-bit pattern layout, CPU predicate
  order, all RMWs and writes, enable semantics, no-xref context, and void ABI.
- Size: `0xb4` bytes, 45 ARM64 instructions.
- Recovered signature: `void serdes_set_pattern(u32 pattern_31_0,
  u32 pattern_63_32, u16 pattern_79_64, int enable)`.

## Semantics

First clears offset `0x94` bits 12-15. It then tests CPU 133 and, only if that
does not equal 1, CPU 129; either matching type clears offset `0x24` bits
16-18.

The function lays out an 80-bit pattern as follows:

| Pattern bits | SerDes destination |
| --- | --- |
| 0-31 | full word at offset `0x9c` |
| 32-63 | full word at offset `0xa0` |
| 64-79 | offset `0xa4` bits 0-15, preserving bits 16-31 |

After loading the pattern, `enable == 1` sets offset `0xa4` bits 16-18 to 7 by
OR; every other value clears those three bits. There are no logs.

## Return Semantics

The final `RET` retains the SerDes base pointer last loaded into `x0`. That
incidental pointer is not derived as an API result, so the recovered semantic
ABI is `void`.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be an
externally consumed diagnostic pattern API.

## Evidence

- Complete ARM64 body at `0x2fe4` through `0x3094`.
- Explicit `AND W20, W20, #0xffff` proving the third argument width in use.
- Exact writes to offsets `0x9c`, `0xa0`, and `0xa4` and final bit 16-18 RMW.
- IDA type at `0x2fe4` updated to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
