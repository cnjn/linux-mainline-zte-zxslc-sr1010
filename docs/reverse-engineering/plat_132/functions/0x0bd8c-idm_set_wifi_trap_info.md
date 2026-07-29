# 0x0bd8c idm_set_wifi_trap_info

## Status

- Status: complete
- Confidence: verified descriptor reads, output writes, debug branches, counter
  updates, return register, and all three callers. Output-field labels remain
  raw where the module provides no semantic name.
- Size: `0x1b0` bytes, 105 ARM64 instructions.
- Recovered signature:
  `u8 idm_set_wifi_trap_info(const void *descriptor, zte_wifi_trap_info_t *output, u32 queue)`.

## Semantics

The helper derives a trap record from one raw RX descriptor. It writes exactly
36 bytes at `output`:

- `+0x0`: the supplied queue.
- `+0x4`: descriptor byte `+0x6 & 0x3f`.
- `+0x8`: descriptor bits 7 through 12, extracted from the little-endian
  16-bit value at `+0x6`.
- `+0xc`: descriptor byte `+0x7.bit5` as a Boolean.
- `+0x10`: whether the supplied queue equals 15.
- `+0x14..+0x23`: an exact 16-byte copy of descriptor bytes `+0x8..+0x17`.

All three in-module callers use stack-resident output storage. `cpu_sw_rx` and
`idm_net_rx` provide a 40-byte stack range, while `pp_tcp_gro_flush` explicitly
zeros the 36 bytes written by this helper and leaves a four-byte alignment gap
before its next stack object. No in-module code reads beyond output `+0x23`; any
receiver expectation beyond the verified 36-byte record is unknown.

Descriptor byte `+0x8` selects two diagnostic paths. Reasons `0x62` and `0x63`
increment the NP1-trap counter; reason `0x65` increments the NP2-RX-trap
counter. Each path dumps data only while its signed debug budget is positive and
`dump_net_check` accepts the raw packet range. On an accepted dump it decrements
that budget, prints the reason and descriptor byte `+0x9`, prints trap-format
descriptor diagnostics, prints the translated buffer address and low 14-bit
length, and emits the packet dump.

The buffer address is reconstructed as
`((u32)descriptor[0] - memstart_addr) | 0xffffff8000000000ULL`. Independently
of the reason/debug path, descriptor byte `+0x7.bit5` increments a third raw
counter. The function returns the unmodified descriptor byte `+0x7`; every
in-module caller ignores it.

## Caller Context

- `idm_net_rx @ 0xbf6c` passes fixed queue 16 before delivering an IDM RX skb
  through `idm_skb_recv`.
- `cpu_sw_rx @ 0xc3ec` passes its CPU RX queue when its raw trap predicate holds.
- `pp_tcp_gro_flush @ 0x10c24` passes a saved 32-byte descriptor snapshot and
  saved queue when routing a GRO aggregate through `idm_skb_recv`.

## Globals and Concurrency

The helper reads `memstart_addr`, `np1_trap_debug`, and `idm_rx_debug`; it
increments three adjacent raw trap counters and decrements a debug budget only
after an accepted filter match. It uses the budget value loaded before the
filter call, so a concurrent writer can be overwritten by the later decrement.
It has no lock, allocation, ownership change, or pointer/length validation. The
caller owns the stack record and the externally registered `idm_skb_recv`
callback receives its pointer.

## Evidence

- Complete 105-instruction ARM64 body at `0xbd8c` through `0xbf38`.
- Exact `LDRB`, `LDRH`, `UBFX`, `LDP`, and `STP` output-record operations.
- Three direct caller xrefs and decompilation of `idm_net_rx`, `cpu_sw_rx`, and
  `pp_tcp_gro_flush`.
- `dump_net_check`, `dump_net_desc`, and `dump_net_data` reconstructions for
  the diagnostic branches.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Protocol meanings of the raw output fields and the queue-15 Boolean.
- Receiver ABI/lifetime for `idm_skb_recv`, including whether it expects bytes
  beyond the verified 36-byte record.
- Hardware meaning and consumers of reasons `0x62`, `0x63`, `0x65`, and
  descriptor byte `+0x7.bit5`.
