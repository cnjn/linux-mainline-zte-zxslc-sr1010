# 0x14be4 idm_wifi_tx

## Status

- Status: complete
- Confidence: verified physical address, short-frame padding, descriptor port
  encoding, barrier/doorbell, and unconditional return.
- Size: `0xf0` bytes, 61 ARM64 instructions.
- Recovered signature:
  `int idm_wifi_tx(struct sk_buff *skb, void *descriptor)`.

## Semantics

The function writes `virt_to_phys(skb + 0x130)` to descriptor word zero and
calls `data_padding(skb)` for length `<= 59`. It clears descriptor word `+0x4`,
writes `+0x8 = 0x00400000`, and encodes low 14 length bits in descriptor
halfword `+0x4`, bits 1 through 14.

It takes `skb + 0x108 & 0x3f` as a port-like value, replaces descriptor byte
`+0x7` low six bits with it, and writes that value shifted left 26 to descriptor
word `+0x10`.

After debug-only dump logic, it executes `DSB ST`, writes `0x20000` through
`qword_29038` (the third observed TX doorbell pointer), and returns zero.

## Caller Context

`idm_net_tx` invokes this backend through IDM ops slot `+0x80` for direct
type-3 IDM netdev TX. Its caller owns descriptor rollback and skb lifetime.

## Evidence

- Complete 61-instruction ARM64 disassembly at `0x14be4` through `0x14cd0`.
- IDM ops-table slot `+0x80` and `idm_net_tx @ 0xd234` call setup.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- Exact descriptor field names and the semantic distinction among the three
  TX doorbells remain unresolved.
- Short-frame padding ownership/length guarantees require `data_padding`
  reconstruction.
