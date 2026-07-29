# 0x1293c smac_del_phy_scan

## Status

- Status: complete
- Confidence: verified single timer deletion, caller, and semantic void ABI.
- Size: `0x20` bytes, 7 ARM64 instructions.
- Recovered signature: `void smac_del_phy_scan(void)`.

## Semantics

Calls `del_timer(&phy_timer)` and ignores its raw return. It does not stop the
PHY scan thread, alter `check_phy_en`, or perform any other teardown itself.

## Caller Context

The sole direct caller is `nppt_exit`, after `idm_exit`. This function therefore
establishes only a timer-deletion portion of PHY scan teardown.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
