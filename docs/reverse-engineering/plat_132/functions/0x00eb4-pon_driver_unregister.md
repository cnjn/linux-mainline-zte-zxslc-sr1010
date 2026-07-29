# 0x00eb4 pon_driver_unregister

## Status

- Status: complete
- Confidence: verified no-argument ABI, fixed driver address, delegated void
  call, and sole direct caller.
- Size: `0x20` bytes, 7 ARM64 instructions.
- Recovered signature: `void pon_driver_unregister(void)`.

## Semantics

Calls `platform_driver_unregister(&zx_pon_driver)` and returns. It does not read
or forward any incoming argument register.

## Caller Context

Its sole direct caller is `plat_cleanupModule @ 0x1c3e8`.

## Evidence

- Complete ARM64 body at `0xeb4` through `0xed0`.
- X0 is overwritten with the fixed driver address before the sole call.
- Exhaustive direct xref query found only the module cleanup wrapper.
- IDA type at `0xeb4` updated to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
