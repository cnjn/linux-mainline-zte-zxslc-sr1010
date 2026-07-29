# 0x0000c setEponDeactiveState

## Status

- Status: complete
- Confidence: verified integer input, boolean normalization, global store,
  incidental pointer return, and absence of direct code xrefs.
- Size: `0x14` bytes, 5 ARM64 instructions.
- Recovered signature: `u32 *setEponDeactiveState(int state)`.

## Semantics

The function stores `state != 0` as a 32-bit zero or one in
`g_epon_deactive`. The address materialized for the store remains in X0, so it
returns `&g_epon_deactive` despite no explicit return-value calculation.

## Caller Context

No direct code xrefs target this entry. It may be an exported or indirect state
update API.

## Concurrency and Ownership

The global write is unsynchronized by this helper. The returned address aliases
module global storage.

## Evidence

- Complete ARM64 body: compare zero, materialize global address, `CSET NE`,
  store, return.
- Exhaustive direct xref query reported no code references.
- IDA type at `0xc` updated to the recovered pointer-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
