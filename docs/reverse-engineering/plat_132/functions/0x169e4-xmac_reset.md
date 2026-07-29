# 0x169e4 xmac_reset

## Status

- Status: complete
- Confidence: verified byte truth test, both mask constants, delegated call,
  void return, and both direct callers.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature: `void xmac_reset(u8 xmac)`.

## Semantics

The function tests the low byte of `xmac` and calls `smac_reset` with:

- `0x400` when the byte is zero.
- `0x800` when the byte is nonzero.

It contains no local register access, error path, or meaningful return value.

## Caller Context

Direct calls occur in:

- `xmac_init_by_work_mode @ 0x17da0`.
- `xmac_config_speed_duplex @ 0x18130`.

Both callers discard the return register.

## Concurrency and Ownership

No local lock, allocation, cleanup, or ownership transfer. Reset behavior is
entirely delegated to `smac_reset`.

## Evidence

- Complete 10-instruction ARM64 body at `0x169e4` through `0x16a08`.
- Exact low-byte `TST` branch and mask constants `0x400`/`0x800`.
- Direct `smac_reset @ 0x12358` call and two caller xrefs.
- IDA function type updated at `0x169e4` to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Exact hardware reset domains represented by masks `0x400` and `0x800`.
