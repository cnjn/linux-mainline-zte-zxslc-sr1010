# 0x04314 serdes_set_rx_eq3

## Status

- Status: complete
- Confidence: verified input validation, EQ3 bitfield locations, ordered RMWs,
  unmasked input shift, residual-return behavior, and export context.
- Size: `0x64` bytes, 24 ARM64 instructions.
- Recovered signature: `void serdes_set_rx_eq3(uint32_t enable, uint32_t equalizer_value)`.

## Semantics

Controls EQ3 in `pon_serdes_base + 0x2c`.

| `enable` | Behavior |
| --- | --- |
| greater than 1 | Logs `serdes rx eq3 en is too big` and performs no MMIO access. |
| 0 | Sets bit 2, disabling EQ3 without changing its data field. |
| 1 | Clears bit 2, then re-reads the word, clears bits `17:13`, and ORs `equalizer_value << 13`. |

`serdes_get_rx_eq` confirms that bit 2 is active-low. The source value is not
masked before its shift, allowing inputs above five bits to spill beyond the
nominal data field.

## MMIO Ordering

The enable path performs two separate RMWs of offset `0x2c`: clear bit 2 and
write, then reread, replace bits 17-13, and write. The disable path only sets
bit 2. Preserve the separate accesses.

## Return Semantics

The machine returns a `printk` result for invalid input, the written register
word for disable, and a residual base pointer for enable. These accidental
values do not form an API contract; the recovered semantic ABI is `void`.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. The vendor
runtime's `system/proc/kallsyms` lists `__ksymtab_serdes_set_rx_eq3` and global
text symbol `serdes_set_rx_eq3 [plat_132]`, confirming it is exported.

## Evidence

- Complete ARM64 body at `0x4314` through `0x4374`.
- Unsigned `CMP W0, #1` / `B.LS 0x4338` rejects values greater than 1.
- Disable path at `0x4364`-`0x4370` sets `0x2c[2]`.
- Enabled path at `0x4344`-`0x435c` clears `0x2c[2]`, then replaces the
  destination field with `W1,LSL#13` after masking only the destination.
- IDA type at `0x4314` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
