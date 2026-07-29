# 0x03098 check_serdes_lock

## Status

- Status: complete
- Confidence: verified independent CPU tests, all MMIO offsets and bits,
  repeated read order, exact logs, no-xref context, and semantic void ABI.
- Size: `0xc8` bytes, 50 ARM64 instructions.
- Recovered signature: `void check_serdes_lock(void)`.

## Semantics

Tests CPU predicates independently and in 132, 133, 129 order. More than one
block could execute if more than one predicate returned exactly 1.

| CPU | Values printed |
| --- | --- |
| 132 | `pon_serdes_pll_base+0x20` bit 0 as `an1_pll_sta`; SerDes `0xd0` bit 0 as `pll_sta`; SerDes `0xe4` bit 1 as `cdr_sta`; SerDes `0xe4` bit 0 as `alos_data` |
| 133 | SerDes `0xd0` bit 0, then `0xe4` bits 1 and 0 |
| 129 | SerDes `0xcc` bit 1, then `0xe4` bits 1 and 0 |

Each block reads offset `0xe4` separately for bit 1 and bit 0; it does not use
one shared snapshot. The function only reports raw bit values and does not
convert them into a combined locked/unlocked result.

## Return Semantics

When CPU 129 does not match, `RET` retains that predicate's result. When it
matches, `RET` retains the final `printk` result. These residual values are not
one coherent API value, so the recovered semantic ABI is `void`.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. It may be a
retained diagnostic entry point.

## Evidence

- Complete ARM64 body at `0x3098` through `0x315c`.
- Three independent predicate calls and exact MMIO load/extraction sequences.
- Separate `LDR` instructions for the two offset-`0xe4` fields in each block.
- IDA type at `0x3098` updated to the recovered void signature.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
