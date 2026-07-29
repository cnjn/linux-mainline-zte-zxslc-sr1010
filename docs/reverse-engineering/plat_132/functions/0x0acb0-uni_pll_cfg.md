# 0x0acb0 uni_pll_cfg

## Status

- Status: complete
- Confidence: verified both unsigned mode ranges, literal CRM profiles,
  ordered RMWs, constant return, and local entry context.
- Size: `0xf8` bytes, 57 ARM64 instructions.
- Recovered signature: `int uni_pll_cfg(uint32_t mode)`.

## Semantics

Programs one of two Uni PLL CRM configurations and always returns zero.

| Mode range | Log | `top_crm_base + 0xc0` | Initial `+0xc4` |
| --- | --- | --- | --- |
| 0 through 4 | `enter epon pon pll cfg` | `0x00202855` | `0x0a000000` |
| 5 through 7 | `enter gpon pon pll cfg` | `0x20202054` | `0x0a2673e3` |
| all other values | none | unchanged | unchanged |

Each selected range first replaces `+0x04` bits 4-5 with `0x20`, writes its
`+0xc0` and `+0xc4` literals, then separately RMW-sets `+0xc4` bit 28, sets
`+0x0c` bit 9, and clears `+0x0c` bit 8. The two tests are separate in machine
code but their unsigned ranges do not overlap.

## Caller Context

There are no internal IDB xrefs to this local text (`t`) entry. Runtime
`kallsyms` does not establish it as an exported API.

## Evidence

- Complete ARM64 body at `0xacb0` through `0xada4`.
- Unsigned mode-zero-through-four gate at `0xacb4`-`0xacc4`; Epon profile at
  `0xacc8`-`0xad24`.
- Unsigned `(mode - 5) <= 2` gate at `0xad28`-`0xad30`; Gpon profile at
  `0xad34`-`0xad94`.
- In both paths, `+0xc4` and `+0x0c` use separate volatile RMWs at
  `0xad04`-`0xad24` and `0xad74`-`0xad94`.
- Constant-zero epilogue at `0xad98`-`0xada4`.
- IDA type at `0xacb0` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
