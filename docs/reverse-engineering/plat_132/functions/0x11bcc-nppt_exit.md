# 0x11bcc nppt_exit

## Status

- Status: complete
- Confidence: verified order, caller, and semantic void ABI.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `void nppt_exit(void)`.

## Semantics

Calls `idm_exit()` first, then `smac_del_phy_scan()`. Both residual return
registers are ignored. It performs no direct IRQ, MMIO, mapping, or netdev
cleanup itself.

## Caller Context

`plat_cleanupModule @ 0x1c3e8` calls this before unregistering the platform
driver. The function establishes teardown order but not the detailed ownership
inside either child.

## Source-Like Reconstruction

`recovered/plat_nppt.c`.
