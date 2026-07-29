# 0x0a154 uni_mode_eth_2p5gbase_r_cfg

## Status

- Status: complete
- Confidence: verified log order, all 49 direct 32-bit profile stores, caller
  mode case, residual-return behavior, and semantic void ABI.
- Size: `0x200` bytes, 102 ARM64 instructions.
- Recovered signature: `void uni_mode_eth_2p5gbase_r_cfg(void)`.

## Semantics

Logs `mode_eth_2p5gbase_r_cfg`, then unconditionally writes an Ethernet
2.5GBASE-R Uni SerDes profile as 49 ordered 32-bit stores at offsets
`0x00..0xc0`, inclusive. It has no CPU predicate, register read, or RMW.

The values match the recovered non-132 2.5GBASE-R profile, but this dedicated
Uni entry applies them unconditionally. Register-field meanings are not
established, so each literal store remains separate.

## Return Semantics

The log result is discarded and X0 retains a base-pointer residual. The
recovered semantic ABI is `void`.

## Caller Context

`uni_serdes_mode_set @ 0xaa90` is the sole internal caller, for mode value four.
The function is local text (`t`) in runtime `kallsyms`, not exported.

## Evidence

- Complete ARM64 body at `0xa154` through `0xa350`.
- Initial log at `0xa158`-`0xa164` precedes all profile writes.
- Ordered direct stores span `0xa180` through `0xa348`, covering every 32-bit
  offset from `0x00` through `0xc0`.
- `uni_serdes_mode_set` calls this profile at `0xaac8` for jump-table case four.
- The disassembly proves separate stores to `+0xa0/+0xa4` at `0xa308`/`0xa314`
  and `+0xbc/+0xc0` at `0xa344`/`0xa348`, despite Hex-Rays merging them.
  IDA instruction comments at `0xa308` and `0xa344` preserve these
  non-merge constraints.
- IDA type at `0xa154` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
