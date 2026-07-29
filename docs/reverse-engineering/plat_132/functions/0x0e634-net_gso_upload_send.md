# 0x0e634 net_gso_upload_send

## Status

- Status: complete
- Confidence: verified descriptor-exhaustion, descriptor setup, GSO parameter
  encoding, barrier/submission, return behavior, and both callers; field labels
  are strong inference.
- Size: `0x154` bytes, 85 ARM64 instructions.
- Recovered signature:
  `int net_gso_upload_send(void *nbuf, struct sk_buff *skb, u32 payload_length, u32 gso_segment_size)`.

## Semantics

The function obtains a TX descriptor from `cpu_tq`. If unavailable, it calls
`cpu_net_free_nbuf(nbuf)`, increments a descriptor-unavailable counter, and
returns `-1`.

On success it configures the descriptor from skb plus nbuf direction bit
`nbuf[0x2c] & 1`, writes the low 32 bits of physical nbuf data pointer
`nbuf + 0x18` at descriptor offset zero, replaces control bits 1 through 14
with nbuf length (`nbuf + 0x28`) masked to 14 bits, clears descriptor words
`+0x10/+0x14`, and ORs `0x0c` into byte `+0x1b`.

When `payload_length > gso_segment_size`, it programs 16-bit GSO fields at
descriptor `+0x12`, `+0x14`, and `+0x16` using ARM `UDIV` quotient/remainder
semantics. ARM division by zero produces quotient zero; this recovery preserves
that behavior rather than introducing C division undefined behavior.

It increments a submission counter, issues `DSB ST`, and returns
`cpu_net_nb_desc_tx(nbuf, descriptor)` unchanged. Debug logging/dumps occur
when `net_gso_debug > 0` but do not mutate state.

## Caller Context

`net_tcp_gso_tx_upload @ 0x0ec3c` and `net_tcp_gso_tx_upload1 @ 0x0ef38` pass
their computed total TCP payload length and GSO segment size as the final two
arguments.

## Concurrency and Ownership

- No local lock; parent GSO sender supplies serialization.
- Descriptor exhaustion hands nbuf to its gated free path. Successful submission
  transfers subsequent nbuf/descriptor ownership to `cpu_net_nb_desc_tx`.

## Evidence

- Complete 85-instruction ARM64 disassembly at `0xe634` through `0xe784`.
- Both caller xrefs and direct upload segmenter argument setup.
- Raw descriptor, physical-address, UDIV/MSUB, barrier, and handoff sequence.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Descriptor field names/bit meanings and `cpu_net_nb_desc_tx` ownership.
- Whether callers ever pass zero segment size outside ARM's defined UDIV result.
