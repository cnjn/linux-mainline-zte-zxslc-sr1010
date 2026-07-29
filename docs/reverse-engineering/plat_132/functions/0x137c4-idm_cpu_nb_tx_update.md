# 0x137c4 idm_cpu_nb_tx_update

## Status

- Status: complete
- Confidence: verified store barrier, 32-bit shift, queue-zero branch, dynamic
  register calculation, and ops-table role.
- Size: `0x38` bytes, 14 ARM64 instructions.
- Recovered signature: `void idm_cpu_nb_tx_update(u32 queue, u32 count)`.

## Semantics

Issues `DSB ST`, computes `count << 17`, and writes it to IDM `+0x80` for queue
zero. Nonzero queues write the same word at offset
`4 * ((queue + 39) & 0x3fffffff)`. Neither input is range checked.

## Caller Context

The only xref is the IDM ops-table slot at `0x26748` (`+0x70`); CPU non-buffer
TX passes its hardware queue and queued count through that callback.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
