# 0x02a58 check_serdes_config

## Status

- Status: complete
- Confidence: verified all mode labels, dump ranges and physical-address
  labels, CPU gate, MMIO access order, and semantic void return.
- Size: `0x198` bytes, 83 ARM64 instructions.
- Recovered signature: `void check_serdes_config(void)`.

## Semantics

This function is a configuration dump, not a validator. It first prints a
label selected by `pon_serdes_mode`:

| Mode | Label |
| --- | --- |
| 0 | `MODE_EPON` |
| 1 | `MODE_10G_EPON_NSYN_DPLL` |
| 2 | `MODE_10G_EPON_NSYN_FIFO` |
| 3 | `MODE_10G_EPON_NSYN_NO_FIFO` |
| 4 | `MODE_10G_EPON_SYN` |
| 5 | `MODE_GPON` |
| 6 | `MODE_XGPON_NSYN` |
| 7 | `MODE_XGPON_SYN` |
| 8 | `MODE_ETH_SGMII` |
| 9 | `MODE_ETH_HSGMII` |
| 10 | `MODE_ETH_USXGMII_2P5G` |
| 12 | `MODE_ETH_USXGMII_5G` |
| 13 | `MODE_ETH_10GBASE_R` |
| 14 | `MODE_ETH_USXGMII_10G` |
| 15 | `MODE_ETH_2P5BASE_X` |
| 16 | `MODE_ETH_1GBASE_X` |
| 11 or any other value | `ERROR MODE` |

It then reads and logs 74 words at SerDes offsets `0x0` through `0x124`,
labeling them as physical addresses `0x16000000` through `0x16000124`. After a
separator, CPU type 132 additionally reads and logs 10 PLL words at offsets
`0x0` through `0x24`, labeled `0x16010000` through `0x16010024`.

## Return Semantics

The non-132 path reaches `RET` with the residual `isCpuType_132` result in
`w0`; the 132 path retains the final `printk` result. Because these paths do
not produce one coherent value and the function is purely diagnostic, the
recovered semantic ABI is `void`.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be a
retained diagnostic entry point.

## Evidence

- Complete ARM64 body at `0x2a58` through `0x2bec`.
- Complete 17-entry switch table, with case 11 mapped to default.
- First loop bound `0x128`, stride 4, and label base `0x16000000`.
- CPU-132 gate and second loop bound `0x28`, stride 4, label base
  `0x16010000`.
- IDA type at `0x2a58` updated to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
