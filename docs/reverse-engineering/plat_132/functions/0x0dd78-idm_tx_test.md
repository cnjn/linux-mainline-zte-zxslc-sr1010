# 0x0dd78 idm_tx_test

## Status

- Status: complete
- Confidence: verified six-register ABI use, fixed template bytes, length and
  port handling, clone loop, error paths, export status, and lack of module
  callers.
- Size: `0x1d4` bytes, 115 ARM64 instructions.
- Recovered signature: `int idm_tx_test(unsigned int port, unsigned int queue,
  void *unused_2, void *unused_3, unsigned int length,
  unsigned int packet_count)`.

## Semantics

The function always copies a fixed 60-byte packet template to its stack, then
returns zero immediately when `packet_count` is zero. Otherwise it allocates an
skb of exactly `length` bytes with raw allocation flags `0xa20`. Allocation
failure logs `"alloc skb failed"` and returns `-1`.

For a successful allocation, it copies `min(length, 60)` template bytes to skb
data. If `length` exceeds 60, it fills the remaining bytes with incrementing
low-byte indexes, wrapping every 256 bytes. It calls `skb_put(skb, length)` and
then redundantly stores `length` in the raw skb length word at offset `0xa8`.

Port handling is exact:

| Port value | Device slot | skb byte `+0x108` |
| --- | --- | --- |
| 7 | PON (`cpu_netdev_slots[0]`) | unchanged |
| `0xffff` | OMCI/OAM (`cpu_netdev_slots[2]`) | `0xff` |
| all other values | switch (`cpu_netdev_slots[1]`) | low byte of `port` |

`queue` is used only in the diagnostic. The third and fourth ABI arguments are
never read. The logged `len` is the template-copy length, capped at 60, rather
than the transmitted skb length.

For a nonzero count, it makes `packet_count - 1` skb copies, sending each copy
through `cpu_net_tx`, then sends the original skb. A copy failure logs
`"skb_copy failed"`, still sends the original skb, and returns `-1`; any earlier
copies remain submitted. No input length, port, queue, or count validation is
present beyond the zero-count fast path.

## Caller Context and Ownership

No direct in-module callers were found. Runtime kallsyms exposes global text
and `__ksymtab_idm_tx_test`, so this is an external test ABI. Each successful
`cpu_net_tx` invocation receives skb ownership under its established TX rules;
this helper does not free or retain an skb after submission.

## Evidence

- Complete ARM64 body at `0x0dd78` through `0x0df48`.
- Register saves at `0xdd8c`, `0xdd9c`, `0xddc4`, and `0xdda4` establish use of
  arguments 0, 1, 4, and 5; X2/X3 are never read.
- Template copy from `0x1e0c0`, exact 60-byte payload, and raw `__alloc_skb`
  argument literals at `0xddcc` through `0xdde8`.
- Length-fill loop at `0xde1c` through `0xde74`; raw skb field stores at
  `0xde3c`, `0xde54`, `0xde94`, and `0xdea0`.
- Port/device branches at `0xde40` through `0xdea4` and repeat-copy sequence at
  `0xdec8` through `0xdf18`.
- No inbound module xrefs; runtime global-text and ksymtab entries.

## Source-Like Reconstruction

`recovered/plat_cpu_tx.c`.

## Open Questions

- The original semantic purpose of ABI arguments three and four.
- The vendor meaning of allocation flags `0xa20` and the fixed test packet.
