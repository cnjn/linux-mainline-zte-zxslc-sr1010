# 0x18328 xmac_aqr_phy_init

## Status

- Status: complete
- Confidence: verified complete body, exact CPU-variant branch, sole caller,
  and discarded return register.
- Size: `0x20` bytes, 8 ARM64 instructions.
- Recovered signature: `void xmac_aqr_phy_init(void)`.

## Semantics

Despite its local runtime symbol name, this function performs no visible AQR
PHY setup. It calls `isCpuType_133()` and returns immediately only when that
result is exactly one. For every other result, it calls `isCpuType_129()` and
then returns.

Neither path writes state, accesses MMIO, logs, allocates, or invokes a PHY
helper. The machine leaves the most recent CPU-predicate result in the return
register, but the sole caller discards it; the recovered interface is semantic
`void`.

## Caller Context

`xmac_init @ 0x18460` is the sole direct in-module caller. It calls this helper
after the RLT and MVL-named stubs, only after its PHY-type-nine early skip, and
ignores the result before continuing to ZXIC setup.

## Evidence

- Complete 8-instruction ARM64 body at `0x18328` through `0x18344`.
- `CMP W0, #1` and `B.EQ` at `0x18334`/`0x18338` establish the exact-one
  condition.
- Direct calls to `isCpuType_133 @ 0x1dc` and, on the non-equal path,
  `isCpuType_129 @ 0x204`.
- Sole code xref at `xmac_init + 0xb0`; runtime kallsyms marks the symbol local
  text.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
