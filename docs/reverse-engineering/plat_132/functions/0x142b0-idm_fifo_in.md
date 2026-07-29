# 0x142b0 idm_fifo_in

## Status

- Status: complete
- Confidence: verified two-argument ABI, inlined softirq-disable/restore
  sequence, lock/full condition, producer write/index update, error path, and
  all direct callers.
- Size: `0x114` bytes, 68 ARM64 instructions.
- Recovered signature: `int idm_fifo_in(u32 fifo_index, uintptr_t buffer)`.

## Semantics

Increments the current `SP_EL0 + 0x10` word by `0x200`, acquires the selected
FIFO lock, then detects full state with the exact unsigned expression
`mask + out - in == 0xffffffff`. A full FIFO is released, has the `0x200`
softirq/preempt delta removed through `__local_bh_enable_ip`, optionally emits a
rate-limited diagnostic, and returns `-1`.

On success it stores `buffer` at `fifo->buffer[in & mask]`, increments `in`,
releases the byte lock with `STLRB` semantics, restores the same `0x200` delta,
unchecked.

## Caller Context

Called by `idm_free_buf @ 0x143c4`, `idm_free_skb_data @ 0x14604`, and two
buffer-population paths in `idm_init @ 0x14ff4`.

## Evidence

- Complete ARM64 body at `0x142b0` through `0x143c0`.
- Inlined `SP_EL0 + 0x10` add at `0x142ec` through `0x142fc`.
- Full detection, release/restore/log path at `0x1430c` through `0x14360`.
- Producer store and input counter update at `0x14364` through `0x143a0`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- The exact kernel meaning of the `0x200` current-context delta is not inferred
  beyond its matched `__local_bh_enable_ip` restoration.
