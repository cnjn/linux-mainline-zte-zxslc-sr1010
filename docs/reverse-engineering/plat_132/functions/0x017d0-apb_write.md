# 0x017d0 apb_write

## Status

- Status: complete
- Confidence: verified pointer/value ABI, volatile word store, unchanged X0
  pointer return, and absence of direct code xrefs.
- Size: `0x8` bytes, 2 ARM64 instructions.
- Recovered signature: `volatile u32 *apb_write(volatile u32 *address, u32 value)`.

## Semantics

Stores `value` through `address` and returns the same address pointer.

## Caller Context

No direct code xrefs target this entry; it may be an external raw APB API.

## Evidence

- Complete ARM64 body: `STR W1,[X0]; RET`.
- X0 is unchanged across the store, preserving the address as return value.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x17d0` updated to the recovered pointer-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
