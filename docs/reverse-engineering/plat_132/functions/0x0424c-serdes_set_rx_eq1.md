# 0x0424c serdes_set_rx_eq1

## Status

- Status: complete
- Confidence: verified input validation, enable/disable polarity, both ordered
  MMIO RMWs, unmasked value shift, residual-return behavior, and export
  context.
- Size: `0x64` bytes, 24 ARM64 instructions.
- Recovered signature: `void serdes_set_rx_eq1(uint32_t enable, uint32_t equalizer_value)`.

## Semantics

Controls EQ1 in `pon_serdes_base + 0x2c`.

| `enable` | Behavior |
| --- | --- |
| greater than 1 | Logs `serdes rx eq1 en is too big` and performs no MMIO access. |
| 0 | Sets bit 0, disabling EQ1 while leaving its data field unchanged. |
| 1 | Clears bit 0, then re-reads the word, clears bits `7:3`, and ORs `equalizer_value << 3`. |

`serdes_get_rx_eq` confirms the polarity: bit 0 clear means EQ1 enabled and
bit 0 set means disabled. The data write deliberately does not mask
`equalizer_value` before shifting. Values above five bits can therefore affect
bits outside `7:3`; this is observed vendor behavior, not a recovered range
check.

## MMIO Ordering

The enabled path has two distinct reads and writes of offset `0x2c`:

1. Read, clear bit 0, write.
2. Read again, clear bits 3-7, OR the unmasked shifted input, write.

The reconstruction must not combine these operations or cache the first read.

## Return Semantics

The machine return register is incoherent: invalid input returns the `printk`
result, `enable == 0` returns the word written to offset `0x2c`, and
`enable == 1` returns the low 32 bits of a residual base pointer. This setter
has no coherent result contract, so its recovered semantic ABI is `void`.

## Caller Context

No direct code or data xrefs target this entry in the current IDB. The vendor
runtime's `system/proc/kallsyms` contains both `__ksymtab_serdes_set_rx_eq1`
and global text symbol `serdes_set_rx_eq1 [plat_132]`, establishing that it is
exported.

## Evidence

- Complete ARM64 body at `0x424c` through `0x42ac`.
- Unsigned `CMP W0, #1` / `B.LS 0x4270` rejects exactly values greater than 1.
- Disable path at `0x429c`-`0x42a8` sets `0x2c[0]`.
- Enabled path at `0x427c`-`0x4294` separately clears `0x2c[0]` and replaces
  the data field with `W1,LSL#3` after masking only the destination.
- IDA type at `0x424c` set to the recovered semantic void signature and
  Hex-Rays cache invalidated after verification.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_smac.c`.
