# 0x0ada8 uni_eth_mode_change

## Status

- Status: complete
- Confidence: verified unsigned range gate, every jump-table result, default
  log/zero behavior, recovered unsigned ABI, and sole caller.
- Size: `0x8c` bytes, 34 ARM64 instructions.
- Recovered signature: `uint32_t uni_eth_mode_change(uint32_t mode)`.

## Semantics

Maps Uni mode values to the PON clock-reset mode consumed by the caller:

| Input | Result |
| --- | --- |
| 0 | 13 |
| 1 | 14 |
| 2 | 11 |
| 3 | 12 |
| 4 | 10 |
| 5 | 9 |
| 6 | 15 |
| 7 | 8 |
| 8 | 16 |

All other unsigned values log
`uni_eth_mode_change error ! Can't find this mode[%#x]!` and return zero.

## Caller Context

`uni_serdes_init @ 0xae34` is the sole direct caller at `0xaf14`. It invokes
`zx_pon_clk_reset_init` only when this mapping returns nonzero. The function is
local text (`t`) in runtime `kallsyms`.

## Evidence

- Complete ARM64 body at `0xada8` through `0xae30`.
- Unsigned mode-bound gate at `0xada8`-`0xadac` and byte jump table at
  `0xadb0`-`0xadc4`.
- Mode-specific immediate returns at `0xadc8` through `0xae30` establish all
  nine entries.
- Default log at `0xae08`-`0xae1c` followed by explicit zero at `0xae20`.
- Caller test and nonzero-gated clock-reset call at `0xaf14`-`0xaf1c`.
- IDA type at `0xada8` set to the recovered unsigned signature and Hex-Rays
  cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
