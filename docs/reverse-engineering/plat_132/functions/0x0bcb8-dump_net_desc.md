# 0x0bcb8 dump_net_desc

## Status

- Status: complete
- Confidence: verified raw descriptor reads, all three printk forms, alternate
  second-argument branch, callers, and semantically unused residual return
  register; field meanings beyond printed labels are unknown.
- Size: `0xd4` bytes, 53 ARM64 instructions.
- Recovered signature: `void dump_net_desc(const void *descriptor, int trap_detail_format)`.

## Semantics

Prints a raw descriptor diagnostic without modifying it. The first two lines
always report:

- Descriptor address, 32-bit words `+0x0` and `+0x4`, byte `+0x6 & 0x3f`, low
  14 bits of word `+0x4`, byte `+0x5.bit7`, and byte `+0x5.bit6`.
- Four raw `soft define` words at `+0x8`, `+0xc`, `+0x10`, and `+0x14`.

When the second argument is nonzero, the final line prints byte `+0x8` as trap
reason, byte `+0x9` as detail, and byte `+0xa & 0x3f` as SSID. Otherwise it
prints byte `+0x8`, byte `+0xd`, the little-endian 16-bit value at `+0xe`, and
bits 0 through 4 of byte `+0xe` under the binary's L3/IP/UDP/TCP labels.

There is no pointer/length validation, filtering, global access, lock,
allocation, ownership change, or semantic return value.

## Caller Context

Five direct callers are debug paths in `idm_set_wifi_trap_info`, `idm_net_rx`,
`cpu_omci_rx`, and `cpu_net_rx`. The alternate nonzero format is used for trap
metadata; its exact protocol interpretation remains unresolved.

## Evidence

- Complete 53-instruction ARM64 body at `0xbcb8` through `0xbd88`.
- Exact printed strings and raw byte/word loads.
- Five direct IDA caller xrefs and no direct callee functions beyond printk.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Hardware/protocol semantics of printed descriptor words and flags.
- Exact meaning of the nonzero format selector and the reported SSID field.
