# 0x09bdc uni_serdes_set_clk_change

## Status

- Status: complete
- Confidence: verified zero/nonzero branch behavior, volatile RMW masks, log
  strings, returned log result, and exported ABI.
- Size: `0x4c` bytes, 17 ARM64 instructions.
- Recovered signature:
  `int uni_serdes_set_clk_change(uint32_t use_local_clock)`.

## Semantics

Selects the Uni SerDes RX-to-TX clock source through bit 18 of
`uni_serdes_base + 0x48`:

| Input | MMIO operation | Log |
| --- | --- | --- |
| zero | Clear bit 18 | `set rx to tx  clk looptiming clk` |
| nonzero | Set bit 18 | `set rx to tx  clk local clk` |

Each branch performs one volatile read-modify-write and returns the final
`printk` result. The source-like name reflects the vendor log wording only;
the hardware clock-source semantics are otherwise unverified.

## Caller Context

No internal IDB xrefs target this exported entry. Runtime `kallsyms` exposes
`__ksymtab_uni_serdes_set_clk_change`.

## Evidence

- Complete ARM64 body at `0x9bdc` through `0x9c24`.
- `CBNZ W0` at `0x9bec` distinguishes zero from every nonzero value.
- Zero path RMW at `0x9bf0`-`0x9bf8` clears with `0xfffbffff`.
- Nonzero path RMW at `0x9c08`-`0x9c10` sets with `0x40000`.
- Shared returned `printk` call at `0x9c1c`.
- IDA type at `0x9bdc` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
