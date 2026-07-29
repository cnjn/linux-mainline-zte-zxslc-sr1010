# 0x13d4c idm_rx_refill_reuse

## Status

- Status: complete
- Confidence: verified pool branch, pre-increment slot selection, separate wrap
  bounds, release-store order, byte swap, and both direct callers.
- Size: `0xac` bytes, 43 ARM64 instructions.
- Recovered signature: `void idm_rx_refill_reuse(u32 old_buffer, int pool)`.

## Semantics

Locks `idm_refill_lock`, selects the current normal ring slot when `pool == 0`
`uIDM_RX_JUMBO_BP_NUM`. It releases the lock with `STLRB` semantics, then writes
the byte-swapped `old_buffer` to the selected slot after unlocking.

Neither `old_buffer` nor `pool` is validated. The byte-swap result retained in
`W0` is not a semantic return contract.

## Caller Context

`idm_rx_refill0 @ 0x14144` calls this helper for explicit reuse and allocation
failure with a nonzero old buffer.

## Evidence

- Complete ARM64 body at `0x13d4c` through `0x13df4`.
- Direct callers at `0x14158` and `0x1418c`.
- Slot address formation before lock release and `REV W0,W0` helper call after
  release.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
