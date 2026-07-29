# 0x0417c serdes_set_pll_open_loop

## Status

- Status: complete
- Confidence: verified exact branch predicate, all three ordered
  read-modify-write operations in each path, log strings, return behavior, and
  export context.
- Size: `0x84` bytes, 31 ARM64 instructions.
- Recovered signature: `int serdes_set_pll_open_loop(uint32_t enable)`.

## Semantics

This setter opens the PLL loop only when `enable == 1`. Every other 32-bit
input, including zero, closes it. It always reads `pon_serdes_base + 0x68`
once before the branch; that first value supplies the first write of either
path.

| Condition | `0x68[10:9]` | `0x68[22]` | `0x74[13]` | Log |
| --- | --- | --- | --- | --- |
| `enable == 1` | set | set | set | `open pll open loop en =0x%x` |
| `enable != 1` | clear | clear | clear | `close pll open loop en =0x%x` |

The binary does not combine the two writes to offset `0x68`: it writes the
first-read value after changing bits 10-9, then performs a second volatile
read to change bit 22. The reconstruction preserves that sequence. Offset
`0x74` is read and written after both offset-`0x68` operations.

## Return Semantics

Each path finishes at the shared `BL printk` at `0x41f4`; its result remains in
`w0` through `RET`. The recovered function therefore returns the logging
result, unlike the neighboring `serdes_set_tx_eq`, which explicitly returns
zero.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. The vendor
runtime's `system/proc/kallsyms` contains both
`__ksymtab_serdes_set_pll_open_loop` and global text symbol
`serdes_set_pll_open_loop [plat_132]`, establishing that it is exported.

## Evidence

- Complete ARM64 body at `0x417c` through `0x41fc`.
- `CMP W0, #1` and `B.NE 0x41cc` establish the exact-open predicate.
- Open path at `0x4194`-`0x41c4`: set `0x68[10:9]`, reread/set `0x68[22]`,
  then set `0x74[13]`.
- Close path at `0x41cc`-`0x41e8`: clear the same fields in the same order.
- Both paths use `W1`, populated at `0x417c`, as the log argument at the
  shared `printk` call.
- IDA type at `0x417c` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
