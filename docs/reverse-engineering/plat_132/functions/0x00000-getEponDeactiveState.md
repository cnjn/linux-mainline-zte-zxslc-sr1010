# 0x00000 getEponDeactiveState

## Status

- Status: complete
- Confidence: verified full 32-bit global read, unsigned return, and absence of
  direct code xrefs.
- Size: `0xc` bytes, 3 ARM64 instructions.
- Recovered signature: `u32 getEponDeactiveState(void)`.

## Semantics

Returns the complete 32-bit value of global `g_epon_deactive`.

## Caller Context

No direct code xrefs target this entry. It may be an exported or indirect state
query API.

## Concurrency and Ownership

The global read is unsynchronized by this helper.

## Evidence

- Complete ARM64 body: global `LDR W0` followed by `RET`.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x0` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
