# 0x05ba8 mode_gpon_syn_cfg

## Status

- Status: complete
- Confidence: verified CPU-129-only gate, all 49 ordered 32-bit profile
  stores, inherited vendor log string, no-xref context, and semantic void ABI.
- Size: `0x1e8` bytes, 103 ARM64 instructions.
- Recovered signature: `void mode_gpon_syn_cfg(void)`.

## Semantics

This function is a CPU-129-only synchronous GPON profile script. It prints the
vendor string `mode_gpon_cfg` rather than its own symbol name, checks
`isCpuType_129() == 1`, and does no MMIO access when the predicate does not
match. On CPU 129 it writes 49 raw 32-bit values in order at offsets
`0x00..0xc0` of `pon_serdes_base`.

The profile includes the fixed tail `0xac=0x201c`, `0xb0=0x0c`,
`0xb4=0x01000000`, `0xb8=0x80`, `0xbc=0x10000`, and `0xc0=0`. The last two
hardware writes are separate 32-bit accesses despite Hex-Rays displaying them
as a QWORD store.

## Return Semantics

The log result is discarded. CPU 129 retains a base-pointer residual; other
CPUs retain the predicate result. The recovered semantic ABI is `void`.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. Vendor
kallsyms lists lowercase local text symbol `mode_gpon_syn_cfg [plat_132]` and
has no `__ksymtab_mode_gpon_syn_cfg` entry, so it is module-private.

## Evidence

- Complete ARM64 body at `0x5ba8` through `0x5d8c`.
- CPU-129 gate at `0x5bbc`-`0x5bc4`.
- All profile stores are `STR W` instructions at `0x5be0`-`0x5d84`.
- IDA type at `0x5ba8` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
