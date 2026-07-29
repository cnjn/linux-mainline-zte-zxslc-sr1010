# 0x1b08c zte_gephy_set_ref_clk_25M

## Status

- Status: complete
- Confidence: verified low-bit input, global APB base, fixed offset/field,
  delegated return, and no direct xrefs.
- Size: `0x30` bytes, 12 ARM64 instructions.
- Recovered signature:
  `volatile u32 *zte_gephy_set_ref_clk_25M(u8 enable)`.

## Semantics

The helper returns the direct APB address pointer from:

```c
apb_bit_write(gephy_apb_base + 0x200018, enable & 1U, 1U, 12U);
```

## Caller Context

No direct code xrefs target this entry in the module. It may be retained for an
external or indirect GEPHY control path; direct xrefs cannot establish that.

## Concurrency and Ownership

No local lock, allocation, cleanup, or base-pointer validation exists. Field
serialization is delegated to `apb_bit_write`, if any.

## Evidence

- Complete ARM64 body at `0x1b08c` through `0x1b0b8`.
- Exact global base, offset `0x200018`, low-bit mask, width one, bit offset 12,
  and tail return of `apb_bit_write`.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1b08c` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Hardware meaning of APB bit 12.
