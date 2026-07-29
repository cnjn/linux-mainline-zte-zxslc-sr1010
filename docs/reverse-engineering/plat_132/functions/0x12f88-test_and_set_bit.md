# 0x12f88 test_and_set_bit

## Status

- Status: complete
- Confidence: verified 64-bit word selection, bit masking, fast path, exclusive
  retry, barrier, old-bit return, and all direct callers.
- Size: `0x80` bytes, 32 ARM64 instructions including stack protector.
- Recovered signature:
  `int test_and_set_bit(u32 bit, volatile unsigned long *address)`.

## Semantics

Selects `address + 8 * (bit >> 6)` and mask `1UL << (bit & 63)`. If an ordinary
initial read already finds the bit set, returns one without entering the
exclusive loop. Otherwise it performs `LDXR`/`STLXR` retries to OR the bit into
the selected 64-bit word, issues `DMB ISH`, and returns whether the successful
exclusive-load value already contained the bit.

Thus a racing setter can make the slow path return one even though the initial
ordinary read was clear. The supplied bit/address are unchecked. The compiler
stack-protector sequence has no semantic role in the bit operation.

## Caller Context

Four direct calls are inside `_check_abuf @ 0x15e98`; no other module code xref
targets this local entry.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
