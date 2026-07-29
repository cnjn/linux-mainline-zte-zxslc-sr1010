# 0x1693c phy_dynamic_identify

## Status

- Status: complete
- Confidence: verified lookup order, global writes, unchanged fallback state,
  and sole caller.
- Size: `0x48` bytes, 16 ARM64 instructions.
- Recovered signature: `void phy_dynamic_identify(void)`.

## Semantics

Looks up `aal_phy_enable_set` first. If present, it writes `g_phy_type = 0` and
returns without attempting another lookup. If absent, it looks up
`phy_common_c45_enable_set`; if that is present, it writes `g_phy_type = 1`.
When neither symbol is available, it leaves `g_phy_type` unchanged. The residual
lookup result in the return register is not an evidenced return contract.

## Caller Context

`xmac_init @ 0x18460` calls it at `0x18504` after excluding PHY type 9, then
ignores the result. There are no other direct module callers.

## Evidence

- Complete ARM64 body at `0x1693c` through `0x16980`.
- Literal lookup names at `0x24353` and `0x24366`.
- Direct caller xref at `0x18504`.
- All `g_phy_type @ 0x26774` xrefs originate from this function.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
