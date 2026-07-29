# 0x1295c nppt_smac_set_uni_mode

## Status

- Status: complete
- Confidence: verified offset arithmetic, field-clear mask, raw mode OR, void
  return, and sole caller.
- Size: `0x24` bytes, 9 ARM64 instructions.
- Recovered signature: `void nppt_smac_set_uni_mode(u32 mac, u32 mode)`.

## Semantics

The function computes a volatile-word offset as:

```c
offset = 4 * ((mac + 5) & 0x3fffffff);
```

It reads `nppt_base + offset`, clears bits `25:23` using mask `0xfc7fffff`, ORs
the complete raw `mode` value without masking it, and writes the result back.
It performs no selector or mode validation and has no meaningful return value.

## Caller Context

The sole direct caller is `nppt_smac_init @ 0x129c8` at `0x12b40`, which passes
mode zero for each initialized normal SMAC selector.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. This is a volatile
single-word read-modify-write; caller serialization controls concurrent access.

## Evidence

- Complete 9-instruction ARM64 body at `0x1295c` through `0x1297c`.
- Exact `mac + 5`, 30-bit `UBFIZ` mask/scale, field-clear mask, and raw mode OR.
- One direct caller xref from `nppt_smac_init`.
- IDA function type updated at `0x1295c` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of the cleared bits and whether valid modes are expected to
  be confined to that field.
