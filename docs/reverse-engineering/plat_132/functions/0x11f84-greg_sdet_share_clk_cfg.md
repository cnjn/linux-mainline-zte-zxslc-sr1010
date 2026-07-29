# 0x11f84 greg_sdet_share_clk_cfg

## Status

- Status: complete
- Confidence: verified unsigned validation, RMW, log, return values, and caller.
- Size: `0x48` bytes, 18 ARM64 instructions.
- Recovered signature: `int greg_sdet_share_clk_cfg(u32 enable)`.

## Semantics

For inputs zero and one, replaces NPPT `+0x19c` bit zero with the input and
returns that input as `int`. Every unsigned input above one logs
`PARA ERROR:greg_sdet_share_clk_cfg set failed! <para:%#x>` and returns `-1`
without reading or writing the register.

## Caller Context

`ponserdes_to_xmac1_en_set` calls it only for inputs zero and one, forwarding
its original `enable` argument and intentionally ignoring this return.

## Source-Like Reconstruction

`recovered/plat_nppt.c`; the caller declaration/call in `recovered/plat_smac.c`
was corrected to match the binary ABI.
