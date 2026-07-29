# 0x00df4 pps_reset

## Status

- Status: complete
- Confidence: verified register address, reset/restore masks, delay, log order,
  zero return, and absence of direct code xrefs.
- Size: `0x98` bytes, 33 ARM64 instructions.
- Recovered signature: `int pps_reset(void)`.

## Semantics

Reads PPS offset `0xc` and logs its value/address. It masks the word with
`0xfff85400`, writes and logs that reset value, ORs `0x0007abff`, delays
`1718000`, writes/logs the restored value, and returns zero.

## Concurrency and Ownership

The multi-step reset sequence is not locally serialized against other PPS
register writers.

## Evidence

- Complete ARM64 body at `0xdf4` through `0xe88`.
- Exact offset, masks, delay constant, strings, and zero return.
- Exhaustive direct xref query reported no code references.
- IDA type at `0xdf4` updated to the recovered no-argument signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
