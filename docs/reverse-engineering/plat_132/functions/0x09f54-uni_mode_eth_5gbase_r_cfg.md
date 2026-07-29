# 0x09f54 uni_mode_eth_5gbase_r_cfg

## Status

- Status: complete
- Confidence: verified log order, all 49 direct 32-bit profile stores, caller
  mode cases, residual-return behavior, and semantic void ABI.
- Size: `0x200` bytes, 102 ARM64 instructions.
- Recovered signature: `void uni_mode_eth_5gbase_r_cfg(void)`.

## Semantics

Logs `mode_eth_5gbase_r_cfg`, then unconditionally writes an Ethernet 5GBASE-R
Uni SerDes profile as 49 ordered 32-bit stores at offsets `0x00..0xc0`,

The values match the previously recovered CPU-133 5GBASE-R profile, but this
dedicated Uni entry applies them without a CPU gate. Register-field semantics
are unknown, so every literal store remains individual.

## Return Semantics

The log result is discarded and X0 retains a base-pointer residual. No result
contract is established; the recovered semantic ABI is `void`.

## Caller Context

`uni_serdes_mode_set @ 0xaa90` is the sole internal caller, for mode values two
and three. The function is local text (`t`) in runtime `kallsyms`, not exported.

## Evidence

- Complete ARM64 body at `0x9f54` through `0xa150`.
- Initial log at `0x9f58`-`0x9f64` precedes all profile writes.
- Ordered direct stores span `0x9f80` through `0xa148`, covering every 32-bit
  offset from `0x00` through `0xc0`.
- `uni_serdes_mode_set` calls this profile at `0xaac0` for jump-table cases two
  and three.
- The disassembly proves separate stores to `+0xa0/+0xa4` at `0xa108`/`0xa114`
  and `+0xbc/+0xc0` at `0xa144`/`0xa148`, despite Hex-Rays merging them.
  IDA instruction comments at `0xa108` and `0xa144` preserve these
  non-merge constraints.
- IDA type at `0x9f54` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
