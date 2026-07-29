# 0x1bfa4 phy_051_set_xmac_work_mode

## Status

- Status: complete
- Confidence: verified both raw PCS-mode branches, delegated arguments,
  post-call speed selection, default return, and absence of external xrefs.
- Size: `0x68` bytes, 26 ARM64 instructions.
- Recovered signature:
  `int phy_051_set_xmac_work_mode(u8 xmac, u32 pcs_mode)`.

## Semantics

The helper initializes its return status to zero and recognizes only two raw
PCS modes:

- Mode `3` immediately returns `xmac_2pt5gbase_x_conf(xmac)` unchanged.
- Mode `6` calls `xmac_sgmii_conf(xmac, 1, 3, 1)`, then unconditionally calls
  `xmac_set_speed_sel(xmac, 1)`, including when the configuration call failed.
  It returns the configuration call's status.

All other modes return zero without a side effect. The second argument is only
equality-tested, so its original signedness is not independently established;
the reconstruction models raw PCS modes as `u32`.

## Caller Context

No in-module code or data reference targets this function in the current IDB.
It is therefore recorded as an unreferenced local helper rather than assigned a
speculative caller or callback-table role.

## Concurrency and Ownership

No local lock, allocation, cleanup, ownership transfer, direct global write, or
direct MMIO access occurs. Hardware changes are delegated to XMAC helpers.

## Evidence

- Complete 26-instruction ARM64 body at `0x1bfa4` through `0x1c008`.
- Exact equality tests for PCS modes `3` and `6`.
- Exact calls to `xmac_2pt5gbase_x_conf @ 0x17378`,
  `xmac_sgmii_conf @ 0x16ee4`, and `xmac_set_speed_sel @ 0x1670c`.
- `xmac_set_speed_sel(xmac, 1)` follows the mode-six configuration call without
  testing its return value.
- Full xref query shows no external code or data reference.
- IDA function type updated at `0x1bfa4` to the recovered signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.

## Open Questions

- Why this local helper is unreferenced in the analyzed module image.
- Hardware meaning of the raw PCS selectors and the fixed SGMII arguments.
