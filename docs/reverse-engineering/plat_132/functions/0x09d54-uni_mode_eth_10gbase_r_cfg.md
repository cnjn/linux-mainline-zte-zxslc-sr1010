# 0x09d54 uni_mode_eth_10gbase_r_cfg

## Status

- Status: complete
- Confidence: verified log order, all 49 direct 32-bit profile stores, caller
  mode cases, residual-return behavior, and semantic void ABI.
- Size: `0x200` bytes, 102 ARM64 instructions.
- Recovered signature: `void uni_mode_eth_10gbase_r_cfg(void)`.

## Semantics

Logs `mode_eth_10gbase_r_cfg`, then unconditionally writes an Ethernet
10GBASE-R Uni SerDes profile as 49 ordered 32-bit stores at offsets
`0x00..0xc0`, inclusive. There are no CPU-type predicates, register reads, or
read-modify-write operations in this entry.

The profile values match the previously recovered CPU-133 10GBASE-R profile,
but this dedicated Uni entry applies them without a CPU gate. Register-field
semantics remain unknown; each literal store is retained individually.

## Return Semantics

The initial log result is discarded and the machine leaves a base-pointer
residual in X0. No meaningful result contract is established, so the recovered
semantic ABI is `void`.

## Caller Context

`uni_serdes_mode_set @ 0xaa90` is the sole internal caller, for mode values
zero and one. The function is local text (`t`) in runtime `kallsyms`, not an
exported interface.

## Evidence

- Complete ARM64 body at `0x9d54` through `0x9f50`.
- Initial log at `0x9d58`-`0x9d64` precedes the profile writes.
- Ordered direct stores span `0x9d80` through `0x9f48`, covering every
  32-bit offset from `0x00` through `0xc0`.
- `uni_serdes_mode_set` calls this profile at `0xaab8` for jump-table cases
  zero and one.
- `0x9f08`/`0x9f14` are independent stores to `+0xa0` and `+0xa4`, despite
  Hex-Rays rendering a string copy; `0x9f44`/`0x9f48` are likewise independent
  32-bit stores to `+0xbc` and `+0xc0`. IDA instruction comments at `0x9f08`
  and `0x9f44` preserve these non-merge constraints.
- IDA type at `0x9d54` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
