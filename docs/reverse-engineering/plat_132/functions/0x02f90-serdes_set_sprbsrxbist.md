# 0x02f90 serdes_set_sprbsrxbist

## Status

- Status: complete
- Confidence: verified argument flow, 32-bit subtraction, call order, discarded
  helper results, exact log, return behavior, and absence of direct xrefs.
- Size: `0x54` bytes, 21 ARM64 instructions.
- Recovered signature: `int serdes_set_sprbsrxbist(int prbs_mode,
  u32 rx_bist_enable)`.

## Semantics

Performs these calls in order:

1. Calls `serdes_set_rx_prbs_mode` with `prbs_mode - 1` using 32-bit wrapping
   arithmetic.
2. Calls `serdes_set_check_en(rx_bist_enable)`.
3. Calls `serdes_set_err_cnt_en(rx_bist_enable)`.
4. Logs the original mode and enable arguments and returns that `printk`
   result.

The three helper return values are discarded. In particular, `prbs_mode == 0`
passes `0xffffffff` to the unsigned RX-mode helper, which still performs its
CPU-specific setup before rejecting the mode-specific selection.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be an
externally consumed RX-BIST setup API.

## Evidence

- Complete ARM64 body at `0x2f90` through `0x2fe0`.
- `SUB W0, W0, #1` and exact three-call sequence.
- Exact `__func__` string and format argument register setup.
- IDA type at `0x2f90` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
