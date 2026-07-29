# 0x0ae34 uni_serdes_init

## Status

- Status: complete
- Confidence: verified exported ABI, all xmac branches, ordered CRM reset
  sequence, exact delay-loop count, mode mapping gate, swallowed bring-up
  error, and caller context.
- Size: `0x110` bytes, 65 ARM64 instructions.
- Recovered signature: `int uni_serdes_init(uint8_t xmac, uint32_t mode)`.

## Semantics

The entry always returns zero and has distinct paths by `xmac`:

| `xmac` | Behavior |
| --- | --- |
| 0 | Stores `mode` in `uni_serdes_mode`; clears then sets `top_crm_base + 0x70` bits 4 and 5, with ten `__const_udelay(0x418958)` calls after each phase; invokes `uni_zx_serdes_init(mode)` and only logs whether it failed or succeeded. |
| 1 | If `g_ponserdes_to_xmac1 == 1`, maps `mode` through `uni_eth_mode_change` and calls `zx_pon_clk_reset_init` only for a nonzero result. Otherwise logs that PON SerDes is used for PON MAC. |
| other | No action. |

The xmac-zero path intentionally discards the `uni_zx_serdes_init` failure
status after logging it, so the wrapper still returns zero.

## Caller Context

The function is exported through `__ksymtab_uni_serdes_init`. Current IDB has
18 direct call sites, all in XMAC configuration paths including SGMII,
1G/2.5G/5G/10G BASE-R, HSGMII, and USXGMII setup routines.

## Evidence

- Complete ARM64 body at `0xae34` through `0xaf40`.
- xmac-zero gate at `0xae48`; mode store at `0xae4c`-`0xae5c`.
- Ordered `+0x70` clear RMWs at `0xae6c`-`0xae80`, ten-delay loop at
  `0xae84`-`0xae94`, set RMWs at `0xaea8`-`0xaebc`, and second ten-delay loop
  at `0xaec0`-`0xaed0`.
- Underlying bring-up call and result-dependent logs at `0xaed8`-`0xaef4`.
- xmac-one/gate/map/reset path at `0xaef8`-`0xaf20`; occupied-PON log at
  `0xaf24`-`0xaf2c`.
- Explicit constant-zero return at `0xaf30`-`0xaf40`.
- IDA type at `0xae34` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
