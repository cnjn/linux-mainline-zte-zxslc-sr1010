# 0x01154 dg_timer_func

## Status

- Status: complete
- Confidence: verified void callback ABI, power call, all independent mode-bit
  branches, PON offsets/values, flag clear, and timer reference.
- Size: `0xa8` bytes, 42 ARM64 instructions.
- Recovered signature: `void dg_timer_func(void)`.

## Semantics

Calls `hw_power_optx_set(1)`, then independently checks `g_pon_work_mode`:
mask `0xa0` sets PON offset `0x180000` bit zero; bit `0x100` sets offset
`0x1c0004` bit zero; bit `0x40` ORs nine at offset `0x84000`; mask `0x600`
sets offset `0x58400` bit zero. Finally clears `dg_flag`.

## Caller Context

The sole reference is the callback address passed by `dg_timer_init @ 0x1c400`.

## Concurrency and Ownership

The callback reads shared mode state and performs four unsynchronized PON RMWs.

## Evidence

- Complete ARM64 body at `0x1154` through `0x11f8`.
- Exact power argument, masks, offsets, OR values, and final global clear.
- One data xref from timer initialization.
- IDA type at `0x1154` updated to the recovered void callback signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
