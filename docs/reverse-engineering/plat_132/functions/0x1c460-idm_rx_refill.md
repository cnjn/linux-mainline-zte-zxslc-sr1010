# 0x1c460 _idm_rx_refill

## Status

- Status: complete
- Confidence: verified allocation gate, physical address offset, 32-bit swap,
  descriptor store, returns, and both direct callers.
- Size: `0x50` bytes, 20 ARM64 instructions.
- Recovered signature: `int _idm_rx_refill(u32 *descriptor_address, u32 size)`.

## Semantics

The helper allocates an IDM buffer of `size`. A null allocation returns `-1`
without touching `descriptor_address`. On success it converts
`buffer + uBP_BUFFER_OFFSET + 64` to a physical address, applies the module's
32-bit byte-swap helper, stores the result through `descriptor_address`, and
returns zero.

## Caller Context

`idm_init @ 0x14ff4` has two direct call sites during IDM RX setup.

## Concurrency and Ownership

Buffer lifetime is transferred to IDM allocation/descriptor machinery outside
this helper. The output pointer is unchecked on the successful path.

## Evidence

- Complete ARM64 body at `0x1c460` through `0x1c4ac`.
- Exact allocation size input, null gate, global offset plus `0x40`, physical
  conversion, `__fswab32_1`, and descriptor store.
- Exhaustive direct xref query found two `idm_init` call sites.
- IDA type at `0x1c460` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Ownership and release path for the allocated buffer after descriptor posting.
