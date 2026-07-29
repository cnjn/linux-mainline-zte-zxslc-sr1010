# 0x14a30 idm_cpu_tx

## Status

- Status: complete
- Confidence: verified descriptor writes, padding/oversize branches, port
  encoding, barrier/doorbell, and unconditional return.
- Size: `0x15c` bytes, 86 ARM64 instructions.
- Recovered signature:
  `int idm_cpu_tx(struct sk_buff *skb, void *descriptor)`.

## Semantics

The backend writes `virt_to_phys(skb + 0x130)` to descriptor word zero. It calls
`data_padding(skb)` when `skb + 0xa8 <= 59`. If the resulting length exceeds
`0x3fff`, it rate-limits an invalid-length message, calls `dump_stack`, and
forces the skb length to 60.

It clears descriptor word `+0x4`, writes `+0x8 = 0x00400000`, then replaces
bits 1 through 14 of descriptor halfword `+0x4` with the low 14 length bits.

For skb byte `+0x108`:

| Port value | Descriptor `+0x7` low 6 | Descriptor `+0xa` low 6 |
| --- | --- | --- |
| `<= 14` | `0x0f` | port value |
| `> 14` | `0x20` | port low 4 bits |

Finally it executes `DSB ST`, writes `0x20000` to the first IDM TX doorbell
pointer (`qword_29030`), and returns zero unconditionally.

## Caller Context

`cpu_net_tx` installs this function in ops slot `+0x68` and uses it for direct
SW/PON transmission. It may be reached through external consumers of the IDM
ops table as well; the function itself has no descriptor-null or skb-length
allocation guard.

## Evidence

- Complete 86-instruction ARM64 disassembly at `0x14a30` through `0x14b88`.
- IDM ops-table entry at `0x26740`.
- Direct caller/ops-table context from `cpu_net_tx @ 0xd668`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- Exact descriptor field names and the hardware semantics of low-6 port values
  `0x0f` and `0x20` remain unknown.
- The safety of length mutation after physical data address emission relies on
  `data_padding`/hardware behavior not established here.
