# 0x1397c get_order

## Status

- Status: complete
- Confidence: verified 64-bit decrement/shift, zero path, CLZ calculation, and
  no direct module callers.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature: `int get_order(unsigned long size)`.

## Semantics

Computes `pages = (size - 1) >> 12`. It returns zero when `pages` is zero;
otherwise it returns `64 - clz(pages)`, the ARM64 page-order value. Size zero is
not special-cased before the subtraction and therefore follows unsigned
wraparound behavior.

## Caller Context

No direct module code xref targets this local entry, and no export record is
present in this module.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
