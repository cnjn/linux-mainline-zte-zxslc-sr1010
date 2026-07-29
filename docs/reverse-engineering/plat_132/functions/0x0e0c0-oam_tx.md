# 0x0e0c0 oam_tx

## Status

- Status: complete
- Confidence: verified.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature:
  `int oam_tx(const void *data, unsigned int length)`.

## Role and Return Contract

This global OAM send entry is a fixed-port wrapper around `net_tst_tx`. It
passes the caller's data and length unchanged, supplies port 2, and returns the
helper's 32-bit status unchanged. It adds no validation, allocation, locking,
MMIO, callback, or ownership behavior of its own.

Port 2 is the CPU-net management device slot, registered as `oam` in the
captured P2P runtime and as `omci` for the relevant PON work modes.

## Evidence

- Six-instruction ARM64 body at `0x0e0c0` through `0x0e0d4`.
- `MOV W2,#2` followed by the sole `BL net_tst_tx` at `0x0e0cc`.
- No direct in-module callers; the runtime kallsyms entry is global and the IDB
  contains the corresponding `__ksymtab_oam_tx` export record.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- External caller context and whether OAM users depend on allocation-failure
  normalization by the shared helper.
