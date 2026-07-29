# 0x015f4 pon_is_registered

## Status

- Status: complete
- Confidence: verified cached fast path, all ordered work-mode branches, state
  predicates, global stores, return, and sole direct caller.
- Size: `0xac` bytes, 43 ARM64 instructions.
- Recovered signature: `u32 pon_is_registered(void)`.

## Semantics

Any nonzero cached `pon_registered` returns one immediately. Otherwise the
function evaluates independent work-mode branches in XGPON, GPON, EPON, then
XEPON order. XGPON/GPON require ONU state five; EPON/XEPON require nonzero LLID
state. Each matching branch overwrites the shared flag, so later branches win
when mode bits overlap. The resulting flag is returned.

## Caller Context

Its sole direct caller is `cpu_net_tx @ 0xd668`.

## Concurrency and Ownership

The shared registration cache is read and overwritten without local locking.

## Evidence

- Complete ARM64 body at `0x15f4` through `0x169c`.
- Exact cached fast path, mode masks, helper calls, comparison constants,
  ordered global stores, and caller xref.
- IDA type at `0x15f4` updated to the recovered unsigned-return signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
