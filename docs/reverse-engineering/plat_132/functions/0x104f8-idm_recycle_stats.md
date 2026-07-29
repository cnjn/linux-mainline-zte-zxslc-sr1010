# 0x104f8 idm_recycle_stats

## Status

- Status: complete
- Confidence: verified loop bounds, FIFO/counter strides, raw field accesses,
  print order, and semantic void ABI; counter field labels are grounded in the
  vendor strings and paired FIFO helpers.
- Size: `0x144` bytes, 72 ARM64 instructions.
- Recovered signature: `void idm_recycle_stats(void)`.

## Role

Print an unsynchronized diagnostic snapshot of all four FIFO rings and their
64-byte counter records.

## Semantics

For selections 0 through 3, it prints the matching `fifo_name`, unsigned
`producer - consumer` in-use count, and counters in this order:

`empty`, `out batch`, `out single`, `alloc without lock`, `alloc with lock`,
`alloc fail`, `full`, `in batch`, `in single`, `free without lock`, and
`free with lock`.

The corresponding raw counter offsets are `+0x00`, `+0x04`, `+0x08`, `+0x20`,
`+0x24`, `+0x28`, `+0x0c`, `+0x14`, `+0x10`, `+0x18`, and `+0x1c`. The function
does not acquire a FIFO lock, so individual printed values can reflect different
concurrent states. Final `printk` residuals are not a semantic return contract.

## Evidence

- Complete ARM64 body at `0x104f8` through `0x10638`.
- Four-iteration loop over 32-byte FIFO and 64-byte counter strides.
- Vendor diagnostic strings and every loaded field offset.
- No direct IDA callers; runtime kallsyms marks the function module-local.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.
