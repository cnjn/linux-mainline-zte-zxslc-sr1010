# 0x169d0 xmac_get_nppt_glb_xpcs_speed_duplex_in_usxgmii_mode

## Status

- Status: complete
- Confidence: verified.
- Size: `0x14` bytes, five ARM64 instructions.
- Recovered signature: `void xmac_get_nppt_glb_xpcs_speed_duplex_in_usxgmii_mode(
  uint8_t xmac, uint32_t *speed, uint32_t *duplex, uint32_t *auto_status)`.

## Semantics

Forwards all four arguments unchanged to the SGMII-named status reader. It has
no distinct USXGMII register access, validation, state change, or semantic
return value.

## Caller Context

No direct module callers were found.

## Evidence

- Complete body at `0x169d0` through `0x169e0`.
- Direct forwarding call at `0x169d8`.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
