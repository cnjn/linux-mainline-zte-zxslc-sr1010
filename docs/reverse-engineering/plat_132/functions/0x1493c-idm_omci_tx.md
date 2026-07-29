# 0x1493c idm_omci_tx

## Status

- Status: complete
- Confidence: verified physical-address/length/port descriptor writes, minimum
  length mutation, barrier/doorbell, and return behavior.
- Size: `0xf4` bytes, 61 ARM64 instructions.
- Recovered signature:
  `int idm_omci_tx(struct sk_buff *skb, void *descriptor)`.

## Semantics

The function writes `virt_to_phys(skb + 0x130)` to descriptor word zero. If skb
length at `+0xa8` is at most 14, it changes the length to 15. It then writes:

```text
descriptor + 0x4  = length encoded in bits 1..14, preserving bits 0 and 15
descriptor + 0x8  = 0x00400001
descriptor + 0x7  low 6 bits = 0x0f
descriptor + 0xa  low 6 bits = skb byte +0x108
```

It executes `DSB ST`, writes `0x20000` through `idm_txq_reg` (`qword_29028`),
and returns zero unconditionally.

## Caller Context

`cpu_net_tx` reaches this backend through IDM ops slot `+0x78` for direct
OMCI/OAM transmission. The caller owns descriptor rollback and skb ownership;
this backend has no failure path.

## Evidence

- Complete 61-instruction ARM64 disassembly at `0x1493c` through `0x14a2c`.
- IDM ops-table entry at `0x26750`.
- Direct management TX caller context in `cpu_net_tx @ 0xd668`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- Hardware meaning of descriptor bit zero in `0x00400001`, fixed class `0x0f`,
  and this distinct doorbell is unknown.
- The safety of mutating short skb length without visible padding needs vendor
  hardware/allocator evidence.
