# 0x09b58 uni_serdes_set_pll_open_loop

## Status

- Status: complete
- Confidence: verified exact predicate, ordered volatile RMW operations, log
  strings, returned log result, and exported ABI.
- Size: `0x84` bytes, 31 ARM64 instructions.
- Recovered signature:
  `int uni_serdes_set_pll_open_loop(uint32_t enable)`.

## Semantics

Opens the Uni SerDes PLL loop only when `enable == 1`; every other 32-bit input
closes it. The function first reads `uni_serdes_base + 0x68` once for its first
write in either path.

| Condition | `+0x68[10:9]` | `+0x68[22]` | `+0x74[13]` | Log |
| --- | --- | --- | --- | --- |
| `enable == 1` | set | set | set | `open pll open loop en =0x%x` |
| `enable != 1` | clear | clear | clear | `close pll open loop en =0x%x` |

The two writes to `+0x68` remain distinct: the first uses the initial read for
bits 10:9, then the binary rereads `+0x68` to change bit 22. The `+0x74` RMW
follows both `+0x68` writes. Each path returns its final `printk` result.

## Caller Context

No internal IDB xrefs target this exported entry. Runtime `kallsyms` exposes
`__ksymtab_uni_serdes_set_pll_open_loop`.

## Evidence

- Complete ARM64 body at `0x9b58` through `0x9bd8`.
- `CMP W0, #1` at `0x9b5c` and `B.NE` at `0x9b74` establish the exact-open
  predicate.
- Open path at `0x9b78`-`0x9ba0`: set `+0x68` bits 10:9, reread/set bit 22,
  then set `+0x74` bit 13.
- Close path at `0x9ba8`-`0x9bc4` clears the same fields in the same order.
- Shared `printk` call at `0x9bd0` leaves its return in `w0` through `RET`.
- IDA type at `0x9b58` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
