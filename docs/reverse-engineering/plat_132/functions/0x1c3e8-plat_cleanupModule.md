# 0x1c3e8 plat_cleanupModule

## Status

- Status: complete
- Confidence: verified call order, void teardown ABI, exit-table reference, and
  no-argument ABI.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `void plat_cleanupModule(void)`.

## Semantics

The module-exit wrapper calls `nppt_exit()` first and
`pon_driver_unregister()` second. Both are no-argument void teardown calls.

## Caller Context

One data reference at `0x27620` registers this as the module cleanup entry. No
direct code caller exists.

## Concurrency and Ownership

All teardown ordering and resource ownership are delegated to the two callees.

## Evidence

- Complete ARM64 body at `0x1c3e8` through `0x1c3fc`.
- Consecutive no-argument `BL nppt_exit` and `BL pon_driver_unregister`,
  followed by direct return.
- One module cleanup data reference.
- IDA type at `0x1c3e8` updated to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- (none)
