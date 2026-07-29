# 0x1677c xmac0_wan_port_sel

## Status

- Status: complete
- Confidence: verified write width, input truncation, target offset, and export
  status.
- Size: `0x14` bytes, five ARM64 instructions.
- Recovered signature: `void xmac0_wan_port_sel(uint8_t selection)`.

## Semantics

Zero-extends the low input byte and writes it as one 32-bit word to
`sys_ctrl_base + 0x0f4`. It performs no read-modify-write, validation, lock,
barrier, or semantic return operation.

## Caller Context

There are no direct module callers. Collected runtime kallsyms confirms that it
is exported global text, so consumers are external to this module image.

## Evidence

- Complete ARM64 body at `0x1677c` through `0x1678c`.
- `UXTB W0, W0` then `STR W0, [sys_ctrl_base,#0xf4]`.
- Runtime `__ksymtab_xmac0_wan_port_sel` and global-text kallsyms entries.

## Source-Like Reconstruction

`recovered/plat_smac.c`.
