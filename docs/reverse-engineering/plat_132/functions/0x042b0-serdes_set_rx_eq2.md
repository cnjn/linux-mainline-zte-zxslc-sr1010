# 0x042b0 serdes_set_rx_eq2

## Status

- Status: complete
- Confidence: verified input validation, EQ2 bitfield locations, ordered RMWs,
  unmasked input shift, residual-return behavior, and export context.
- Size: `0x64` bytes, 24 ARM64 instructions.
- Recovered signature: `void serdes_set_rx_eq2(uint32_t enable, uint32_t equalizer_value)`.

## Semantics

Controls EQ2 in `pon_serdes_base + 0x2c`.

| `enable` | Behavior |
| --- | --- |
| greater than 1 | Logs `serdes rx eq2 en is too big` and performs no MMIO access. |
| 0 | Sets bit 1, disabling EQ2 without changing its data field. |
| 1 | Clears bit 1, then re-reads the word, clears bits `12:8`, and ORs `equalizer_value << 8`. |

Bit 1 clear is enabled and bit 1 set is disabled, as confirmed by
`serdes_get_rx_eq`. The input is not limited to five bits before the shift, so
values above `0x1f` can modify bits above the documented EQ2 field.

## MMIO Ordering

For `enable == 1`, offset `0x2c` is read and written twice: first to clear the
the single bit-setting RMW. Do not coalesce these accesses.

## Return Semantics

The binary retains a different incidental value in `w0` for each path: a
`printk` result after validation failure, the written register word for disable,
`void`.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. The vendor
runtime's `system/proc/kallsyms` lists `__ksymtab_serdes_set_rx_eq2` and global
text symbol `serdes_set_rx_eq2 [plat_132]`, confirming it is exported.

## Evidence

- Complete ARM64 body at `0x42b0` through `0x4310`.
- Unsigned `CMP W0, #1` / `B.LS 0x42d4` rejects values greater than 1.
- Disable path at `0x4300`-`0x430c` sets `0x2c[1]`.
- Enabled path at `0x42e0`-`0x42f8` clears `0x2c[1]`, then replaces the
  destination field with `W1,LSL#8` after masking only the destination.
- IDA type at `0x42b0` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
