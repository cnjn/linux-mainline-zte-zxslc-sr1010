# 0x182c4 xmac_rlt_phy_init

## Status

- Status: complete
- Confidence: verified complete body, exact CPU-variant branch, sole caller,
  and discarded return register.
- Size: `0x20` bytes, 8 ARM64 instructions.
- Recovered signature: `void xmac_rlt_phy_init(void)`.

## Semantics

Despite its runtime symbol name, this function performs no visible PHY setup.
It calls `isCpuType_133()` and returns immediately only when that result is
exactly one. For every other result, it calls `isCpuType_129()` and returns.

Neither path writes state, accesses MMIO, logs, allocates, or invokes a PHY
helper. The machine leaves the result of the most recent CPU predicate in the
return register, but its sole caller ignores it; the recovered interface is
therefore semantic `void`, rather than treating that incidental register value
as a return contract.

## Caller Context

`xmac_init @ 0x18460` is the sole direct in-module caller. It calls this helper
only after its PHY-type-nine early skip and discards the result before invoking
the next setup helper.

## Evidence

- Complete 8-instruction ARM64 body at `0x182c4` through `0x182e0`.
- `CMP W0, #1` and `B.EQ` at `0x182d0`/`0x182d4` establish the exact-one
  condition, rather than a generic nonzero test.
- Direct calls to `isCpuType_133 @ 0x1dc` and, on the non-equal path,
  `isCpuType_129 @ 0x204`.
- Sole code xref at `xmac_init + 0xa8`; runtime kallsyms marks the symbol local
  text.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
