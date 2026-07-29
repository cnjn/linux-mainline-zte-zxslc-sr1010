# 0x09ae8 uni_serdes_set_tx_eq

## Status

- Status: complete
- Confidence: verified exact input domain, volatile RMW mask/value pairs, log
  strings, constant return, and exported ABI.
- Size: `0x70` bytes, 26 ARM64 instructions.
- Recovered signature: `int uni_serdes_set_tx_eq(uint32_t tx_eq)`.

## Semantics

Sets the Uni SerDes TX equalization field at `uni_serdes_base + 0x20`.

| `tx_eq` | MMIO operation | Log |
| --- | --- | --- |
| 0 | Replace bits 15:8 with `0x0d` | `set tx 3db pre and post success` |
| 1 | Replace bits 15:8 with `0x1d` | `set tx 6db pre and post success` |
| any other value | No MMIO read, write, or log | none |

Each accepted path reads the volatile 32-bit word once, masks with
`0xffff00ff`, ORs the selected field, and stores the result. All paths return
zero after discarding any `printk` result.

## Caller Context

No internal IDB xrefs target this exported entry. Runtime `kallsyms` exposes
`__ksymtab_uni_serdes_set_tx_eq`.

## Evidence

- Complete ARM64 body at `0x9ae8` through `0x9b54`.
- Exact input gate: `CBZ` at `0x9af0`, `CMP #1` at `0x9af4`, and no-op branch
  at `0x9af8`.
- Value-one RMW at `0x9b04`-`0x9b14` uses `0xffff00ff` and `0x1d00`.
- Value-zero RMW at `0x9b2c`-`0x9b3c` uses `0xffff00ff` and `0x0d00`.
- Shared log call at `0x9b48`; `MOV W0, #0` at `0x9b4c` establishes the
  constant return.
- IDA type at `0x9ae8` set to the recovered signature and Hex-Rays cache
  invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
