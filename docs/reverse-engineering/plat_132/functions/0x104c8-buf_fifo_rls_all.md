# 0x104c8 buf_fifo_rls_all

## Status

- Status: complete
- Confidence: verified.
- Size: `0x30` bytes, 12 ARM64 instructions.
- Recovered signature: `void buf_fifo_rls_all(void)`.

## Semantics

Calls `buf_fifo_rls` sequentially with selections 0, 1, 2, and 3. It ignores all
residual return registers and has no other state access, synchronization, or
error handling. No direct IDA caller targets this entry.

## Evidence

- Complete four-call ARM64 body at `0x104c8` through `0x104f4`.
- Fixed `MOV W0,#0..#3` values before each call.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_tx.c`.
