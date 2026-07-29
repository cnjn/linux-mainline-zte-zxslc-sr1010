# 0x17a50 xmac_test_siwtch_work_mode

## Status

- Status: complete
- Confidence: verified all ten dispatch cases, argument literals, diagnostics,
  unconditional enable, default behavior, and export status.
- Size: `0x188` bytes, 86 ARM64 instructions.
- Recovered signature: `int xmac_test_siwtch_work_mode(uint8_t xmac,
  int work_mode)`.

## Semantics

Dispatches test work-mode values 0 through 9 to the XMAC mode configurators:

| Mode | Operation |
| --- | --- |
| 0 | `xmac_10gbase_r_conf` |
| 1 | `xmac_5gbase_r_conf` |
| 2 | `xmac_1gbase_x_conf` |
| 3 | `xmac_sgmii_conf(xmac, 0, 3, 1)` |
| 4 | `xmac_2pt5gbase_x_conf` |
| 5 | `xmac_10g_usxgmii_auto_conf` |
| 6 | `xmac_5g_usxgmii_auto_conf` |
| 7 | `xmac_2pt5g_usxgmii_auto_conf` |
| 8 | `xmac_hsgmii_conf(xmac, 1)` |
| 9 | `xmac_hsgmii_conf(xmac, 0)` |

Each valid case prints the child return status, then always calls
`xmac_tx_rx_enable(xmac)`, including after a nonzero child result, and returns
that status. Any other signed or unsigned value prints the vendor typo
`"unspport work mode"`, does not enable TX/RX, and returns zero.

## Caller Context

No direct module callers were found. Runtime kallsyms marks it as exported
global text, so consumers are external to this module image.

## Evidence

- Complete ARM64 body at `0x17a50` through `0x17bd4`.
- Ten-case jump table at `0x1e610` and all branch targets.
- Direct calls to each configuration helper and `xmac_tx_rx_enable` at
  `0x17bb0`.
- Runtime `__ksymtab_xmac_test_siwtch_work_mode` and global-text kallsyms
  entries.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
