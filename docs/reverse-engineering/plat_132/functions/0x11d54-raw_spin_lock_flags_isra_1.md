# 0x11d54 do_raw_spin_lock_flags.isra.1.constprop.3

## Status

- Status: complete
- Confidence: verified hard-wired lock, exclusive fast path, slowpath call, and
  semantic void ABI.
- Size: `0x44` bytes, 17 ARM64 instructions.
- Recovered signature: `void do_raw_spin_lock_flags_isra_1_constprop_3(void)`.

## Semantics

Acquires only `nppt_glb_auto_gate_lock`. It prefetches, uses an acquire-exclusive
load/store loop to write one when uncontended, and calls
`queued_spin_lock_slowpath(lock, observed, 0, 1)` when the observed word is
nonzero. It does not save IRQ state itself.

## Caller Context

Only the SOPC auto-gate getter and setter call it; each surrounds it with the
separate DAIF save/restore helpers and byte-width store-release unlock.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
