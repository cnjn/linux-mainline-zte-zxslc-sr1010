# 0x09cf0 uni_serdes_set_rx_eq3

## Status

- Status: complete
- Confidence: verified unsigned validation, EQ3 field locations, ordered
  volatile RMWs, unmasked input shift, incoherent raw return register, and
  exported ABI.
- Size: `0x64` bytes, 24 ARM64 instructions.
- Recovered signature:
  `void uni_serdes_set_rx_eq3(uint32_t enable, uint32_t equalizer_value)`.

## Semantics

Controls EQ3 through `uni_serdes_base + 0x2c`.

| `enable` | Behavior |
| --- | --- |
| greater than 1 | Logs `serdes rx eq3 en is too big` and makes no MMIO access. |
| 0 | Sets bit 2, disabling EQ3 without changing its data field. |
| 1 | Clears bit 2, rereads the word, clears bits 17:13, then ORs `equalizer_value << 13`. |

`uni_serdes_get_rx_eq` establishes the active-low polarity. The source value is
not masked before shifting, so inputs above five bits can spill outside the
nominal data field.

## MMIO Ordering

The enabled path separately reads/writes `+0x2c` to clear bit 2, then rereads
and replaces bits 17:13. The disabled path only sets bit 2.

## Return Semantics

The machine retains a `printk` result for invalid input, the written register
word for disable, or a residual base pointer for enable. These incidental
values do not define an API contract; the recovered semantic ABI is `void`.

## Caller Context

No internal IDB xrefs target this exported entry. Runtime `kallsyms` exposes
`__ksymtab_uni_serdes_set_rx_eq3`.

## Evidence

- Complete ARM64 body at `0x9cf0` through `0x9d50`.
- Unsigned `CMP W0, #1` / `B.LS` at `0x9cf0`-`0x9cf4` rejects values greater
  than one.
- Disable RMW at `0x9d44`-`0x9d4c` sets `+0x2c[2]`.
- Enable RMWs at `0x9d20`-`0x9d38` separately clear `+0x2c[2]` and replace
  the destination field using `W1,LSL#13`.
- IDA type at `0x9cf0` set to the recovered semantic `void` signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
