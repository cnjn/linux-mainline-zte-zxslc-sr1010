# 0x01258 zxic_gpio_set_value

## Status

- Status: complete
- Confidence: verified one-instruction no-op body and absence of direct xrefs;
  external unused-argument convention remains unprovable.
- Size: `0x4` bytes, 1 ARM64 instruction.
- Recovered signature: `void zxic_gpio_set_value(void)`.

## Semantics

Returns immediately without reading registers or changing state.

## Caller Context

No direct code xrefs target this entry. If an external consumer passes arguments,
the stub ignores them; their source-level types cannot be recovered internally.

## Evidence

- Complete ARM64 body: `RET`.
- Exhaustive direct xref query reported no code references.
- IDA type at `0x1258` updated to the evidence-limited void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
