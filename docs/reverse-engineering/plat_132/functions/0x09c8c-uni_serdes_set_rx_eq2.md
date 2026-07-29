# 0x09c8c uni_serdes_set_rx_eq2

## Status

- Status: complete
- Confidence: verified unsigned validation, EQ2 field locations, ordered
  volatile RMWs, unmasked input shift, incoherent raw return register, and
  exported ABI.
- Size: `0x64` bytes, 24 ARM64 instructions.
- Recovered signature:
  `void uni_serdes_set_rx_eq2(uint32_t enable, uint32_t equalizer_value)`.

## Semantics

Controls EQ2 through `uni_serdes_base + 0x2c`.

| `enable` | Behavior |
| --- | --- |
| greater than 1 | Logs `serdes rx eq2 en is too big` and makes no MMIO access. |
| 0 | Sets bit 1, disabling EQ2 without changing its data field. |
| 1 | Clears bit 1, rereads the word, clears bits 12:8, then ORs `equalizer_value << 8`. |

`uni_serdes_get_rx_eq` establishes the polarity: bit 1 clear reports EQ2 as
enabled. The input is not limited to five bits before the shift, so large
values may affect bits outside the destination field.

## MMIO Ordering

For `enable == 1`, offset `+0x2c` is read and written twice: first to clear bit
1, then again to replace bits 12:8. Do not coalesce the accesses.

## Return Semantics

The binary leaves a different incidental value in `w0` per path: a `printk`
result after validation failure, the written word for disable, or a residual
base pointer for enable. The recovered semantic ABI is `void`.

## Caller Context

No internal IDB xrefs target this exported entry. Runtime `kallsyms` exposes
`__ksymtab_uni_serdes_set_rx_eq2`.

## Evidence

- Complete ARM64 body at `0x9c8c` through `0x9cec`.
- Unsigned `CMP W0, #1` / `B.LS` at `0x9c8c`-`0x9c90` rejects values greater
  than one.
- Disable RMW at `0x9ce0`-`0x9ce8` sets `+0x2c[1]`.
- Enable RMWs at `0x9cbc`-`0x9cd4` separately clear `+0x2c[1]` and replace
  the destination field using `W1,LSL#8`.
- IDA type at `0x9c8c` set to the recovered semantic `void` signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
