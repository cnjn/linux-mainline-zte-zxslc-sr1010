# 0x169a0 xmac_get_nppt_glb_xpcs_speed_duplex_in_sgmii_mode

## Status

- Status: complete
- Confidence: verified MMIO address calculation, bit extraction, stores, and
  only direct caller.
- Size: `0x30` bytes, 12 ARM64 instructions.
- Recovered signature: `void xmac_get_nppt_glb_xpcs_speed_duplex_in_sgmii_mode(
  uint8_t xmac, uint32_t *speed, uint32_t *duplex, uint32_t *auto_status)`.

## Semantics

Reads one 32-bit NPPT status word at `nppt_base + 0x90 + 4*xmac`, then stores:

| Output | Extraction |
| --- | --- |
| `speed` | bits `0..2` |
| `duplex` | bit `3` |
| `auto_status` | bit `4` |

All three output pointers and the byte selector are unchecked. The final bit-4
value remains in `W0` but is not an evidenced semantic return contract.

## Caller Context

Its only direct module caller is the USXGMII-named forwarding wrapper at
`0x169d0`.

## Evidence

- Complete body at `0x169a0` through `0x169cc`.
- Exact `UBFIZ` 8-bit selector address calculation and bit extracts.
- Direct caller xref at `0x169d8`.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
