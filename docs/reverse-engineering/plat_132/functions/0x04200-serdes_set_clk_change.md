# 0x04200 serdes_set_clk_change

## Status

- Status: complete
- Confidence: verified zero/nonzero branch behavior, one-bit MMIO RMWs, exact
  vendor log strings, `printk` return semantics, and export context.
- Size: `0x4c` bytes, 17 ARM64 instructions.
- Recovered signature: `int serdes_set_clk_change(uint32_t use_local_clock)`.

## Semantics

Selects the RX-to-TX clock source through bit 18 of
`pon_serdes_base + 0x48`:

| Input | `0x48[18]` | Log |
| --- | --- | --- |
| `use_local_clock == 0` | clear | `set rx to tx  clk looptiming clk` |
| `use_local_clock != 0` | set | `set rx to tx  clk local clk` |

The zero path applies `& 0xfffbffff`; the nonzero path applies
`| 0x40000`. Both perform one volatile 32-bit read-modify-write operation and
preserve every other bit. `looptiming` is retained exactly as it appears in the
vendor string.

## Return Semantics

The branch-specific format pointer reaches the shared `BL printk` at `0x4240`.
No later instruction changes `w0`, so this function returns the logging result.

## Caller Context

There are no direct code or data xrefs in the current IDB. The vendor runtime's
`system/proc/kallsyms` contains `__ksymtab_serdes_set_clk_change` and global
text symbol `serdes_set_clk_change [plat_132]`, establishing that this is an
exported module API.

## Evidence

- Complete ARM64 body at `0x4200` through `0x4248`.
- `CBNZ W0, 0x422c` gives the exact zero/nonzero split.
- Clear path at `0x4214`-`0x421c` uses `AND #0xfffbffff`.
- Set path at `0x422c`-`0x4234` uses `ORR #0x40000`.
- Branches select only different log strings before the shared `printk`.
- IDA type at `0x4200` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
