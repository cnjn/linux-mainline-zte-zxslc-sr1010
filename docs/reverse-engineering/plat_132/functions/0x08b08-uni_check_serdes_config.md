# 0x08b08 uni_check_serdes_config

## Status

- Status: complete
- Confidence: verified mode jump table, deliberate mode-2/default error path,
  complete 74-word dump loop, final log return, and exported ABI.
- Size: `0xf8` bytes, 51 ARM64 instructions.
- Recovered signature: `int uni_check_serdes_config(void)`.

## Semantics

Logs a label from `uni_serdes_mode`:

| Mode | Label |
| --- | --- |
| 0 | `MODE_ETH_10GBASE_R` |
| 1 | `MODE_ETH_USXGMII_10G` |
| 3 | `MODE_ETH_USXGMII_5G` |
| 4 | `MODE_ETH_USXGMII_2P5G` |
| 5 | `MODE_ETH_HSGMII` |
| 6 | `MODE_ETH_2P5BASE_X` |
| 7 | `MODE_ETH_SGMII` |
| 8 | `MODE_ETH_1GBASE_X` |
| 2 or other | `ERROR MODE CONFIG` |

It then reads and logs 74 ordered 32-bit words at `uni_serdes_base +
0x00..0x124`. Log addresses are the fixed literal range `0x16100000..0x16100124`,
independent of the runtime base pointer. It returns the final separator `printk`
result.

## Caller Context

No internal IDB xrefs target this exported diagnostic entry. It is exported
through `__ksymtab_uni_check_serdes_config`.

## Evidence

- Complete ARM64 body at `0x8b08` through `0x8bfc`.
- Unsigned mode bound/jump table at `0x8b1c`-`0x8b3c`; case 2 explicitly shares
  the default error block at `0x8bb8`.
- Mode labels at `0x8b40`-`0x8bc0`.
- Dump loop starts at `0x8bc4`, reads `LDR W2, [X0,X19]` at `0x8bcc`, increments
  by four, and terminates at `0x128` via `0x8bdc`-`0x8be0`.
- Final returned separator log at `0x8bec`.
- IDA type at `0x8b08` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
