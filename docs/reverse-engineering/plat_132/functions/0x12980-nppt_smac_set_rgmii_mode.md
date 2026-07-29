# 0x12980 nppt_smac_set_rgmii_mode

## Status

- Status: complete
- Confidence: verified raw RMW, clock/GREG calls, log, and semantic void ABI.
- Size: `0x48` bytes, 18 ARM64 instructions.
- Recovered signature: `void nppt_smac_set_rgmii_mode(void)`.

## Semantics

Reads NPPT `+0x24`, clears mask `0x03800000`, sets `0x00800000`, then calls
`pon_soc_pon_rgmii_clk_set(0)` and `greg_rgmii_intf_mode_set(1)`. It logs the
literal `0x800000` completion value. No direct code caller is present and the
final `printk` residual is not a semantic return.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
