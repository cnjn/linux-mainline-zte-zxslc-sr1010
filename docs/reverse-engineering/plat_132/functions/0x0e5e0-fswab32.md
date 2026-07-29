# 0x0e5e0 __fswab32

## Status

- Status: complete
- Confidence: verified.
- Size: `0x8` bytes, 2 ARM64 instructions.
- Recovered signature: `u32 __fswab32(u32 value)`.

## Semantics

Executes `REV W0,W0` and returns the input with its four bytes reversed. It has
no global access, allocation, synchronization, callback, MMIO, or error path.

## Caller Context

Seven direct xrefs are present: the physical fall-through from `sub_E5C8` and
six ordinary calls in the GSO/TCP transmit helpers. It is a distinct binary
entry from `__fswab32_0 @ 0x10750` and remains separately represented.

## Evidence

- Complete two-instruction body at `0x0e5e0` through `0x0e5e4`.
- ARM64 `REV W0,W0` exactly implements a 32-bit byte reversal.
- Seven direct caller/fall-through xrefs.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.
