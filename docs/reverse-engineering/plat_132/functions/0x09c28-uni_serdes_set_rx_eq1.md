# 0x09c28 uni_serdes_set_rx_eq1

## Status

- Status: complete
- Confidence: verified unsigned validation, enable/disable polarity, ordered
  volatile RMWs, unmasked value shift, incoherent raw return register, and
  exported ABI.
- Size: `0x64` bytes, 24 ARM64 instructions.
- Recovered signature:
  `void uni_serdes_set_rx_eq1(uint32_t enable, uint32_t equalizer_value)`.

## Semantics

Controls EQ1 through `uni_serdes_base + 0x2c`.

| `enable` | Behavior |
| --- | --- |
| greater than 1 | Logs `serdes rx eq1 en is too big` and makes no MMIO access. |
| 0 | Sets bit 0, disabling EQ1 while leaving its data field unchanged. |
| 1 | Clears bit 0, rereads the word, clears bits 7:3, then ORs `equalizer_value << 3`. |

`uni_serdes_get_rx_eq` establishes the polarity: bit 0 clear reports EQ1 as
enabled. The value shift is intentionally unmasked, so bits above the five-bit
destination field may change for large inputs.

## MMIO Ordering

The enabled path has two independent volatile RMWs:

1. Read, clear bit 0, write.
2. Read again, clear bits 7:3, OR the raw shifted input, write.

## Return Semantics

The raw return register is not a coherent API result: invalid input preserves
the `printk` result, disable returns the written word, and enable retains a
base pointer. The recovered semantic ABI is therefore `void`.

## Caller Context

No internal IDB xrefs target this exported entry. Runtime `kallsyms` exposes
`__ksymtab_uni_serdes_set_rx_eq1`.

## Evidence

- Complete ARM64 body at `0x9c28` through `0x9c88`.
- Unsigned `CMP W0, #1` / `B.LS` at `0x9c28`-`0x9c2c` rejects exactly values
  greater than one.
- Disable RMW at `0x9c7c`-`0x9c84` sets `+0x2c[0]`.
- Enable RMWs at `0x9c58`-`0x9c70` separately clear `+0x2c[0]` and replace
  the destination field using `W1,LSL#3`.
- IDA type at `0x9c28` set to the recovered semantic `void` signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
