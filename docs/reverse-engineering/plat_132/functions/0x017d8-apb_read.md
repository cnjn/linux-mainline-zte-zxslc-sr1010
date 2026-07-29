# 0x017d8 apb_read

## Status

- Status: complete
- Confidence: verified pointer ABI, volatile word load, unsigned return, and
  absence of direct code xrefs.
- Size: `0x8` bytes, 2 ARM64 instructions.
- Recovered signature: `u32 apb_read(const volatile u32 *address)`.

## Semantics

Returns the 32-bit word read through `address`.

## Caller Context

No direct code xrefs target this entry; it may be an external raw APB API.

## Evidence

- Complete ARM64 body: `LDR W0,[X0]; RET`.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x17d8` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
