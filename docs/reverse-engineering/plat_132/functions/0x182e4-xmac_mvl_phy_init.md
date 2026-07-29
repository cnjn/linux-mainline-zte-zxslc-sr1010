# 0x182e4 xmac_mvl_phy_init

## Status

- Status: complete
- Confidence: verified complete branch structure, CPU-predicate call order,
  XMAC-type gate, sole caller, and discarded return register.
- Size: `0x40` bytes, 16 ARM64 instructions.
- Recovered signature: `void xmac_mvl_phy_init(void)`.

## Semantics

Despite its local runtime name, this function performs no visible Marvell PHY
initialization. It calls `isCpuType_133()` first. Only when that result is not
exactly one does it call `isCpuType_129()`. If either predicate returns exactly
one and `g_xmac0_type` equals two, it returns immediately.

Every other path calls `isCpuType_133()` a second time and then returns. There
are no writes, MMIO accesses, diagnostics, allocation, or PHY-helper calls.
The residual `x0` register is either raw type two or the second CPU-predicate
result, but `xmac_init` discards it; the reconstructed interface is therefore
semantic `void`.

## Caller Context

`xmac_init @ 0x18460` is the sole direct in-module caller. It calls this helper
after `xmac_rlt_phy_init`, only when raw PHY type is not nine, and ignores the
return register before continuing to the AQR and ZXIC setup helpers.

## Evidence

- Complete 16-instruction ARM64 body at `0x182e4` through `0x18320`.
- `CMP W0, #1` tests at `0x182f0` and `0x18310`, preserving exact-one predicate
  semantics and short-circuit call order.
- `LDR W0, [g_xmac0_type]`, `CMP W0, #2`, and the early branch at
  `0x182fc` through `0x18308` establish the type-two gate.
- The remaining paths converge on a second `BL isCpuType_133` at `0x18318`.
- Sole code xref at `xmac_init + 0xac`; runtime kallsyms marks the symbol local
  text.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
