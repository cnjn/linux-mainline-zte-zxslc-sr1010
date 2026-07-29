# 0x0e5e8 virt_to_phys

## Status

- Status: complete
- Confidence: verified predicate, imported-global reads, both arithmetic paths,
  and direct callers; mapping-class names are intentionally conservative.
- Size: `0x4c` bytes, 19 ARM64 instructions.
- Recovered signature: `u64 virt_to_phys(const void *virtual_address)`.

## Semantics

The helper reads the low 32 bits of imported `vabits_actual`, computes:

```c
direct_map_limit = 0x8000000000ULL - (1ULL << (vabits_actual - 1));
```

and compares that value unsigned against
`(address ^ 0xffffff8000000000ULL)`. When the comparison is greater-or-equal,
it returns `address - kimage_voffset`. Otherwise it returns the low 39 address
bits plus imported `memstart_addr`:

```c
(address & 0x7fffffffffULL) + memstart_addr
```

No range validation, page alignment, cache maintenance, or DMA mapping occurs.
The shift count is used as supplied by the imported runtime value.

## Caller Context

Direct callers are `net_gso_upload_send @ 0x0e634` and two paths in
`net_tcp_gso_tx @ 0x0f258`. Recovered IDM TX backends also establish why the
module needs physical data addresses before descriptor submission, but no
additional direct IDA xref to this local entry is asserted here.

## Evidence

- Complete ARM64 body at `0x0e5e8` through `0x0e630`.
- Imported reads of `vabits_actual`, `memstart_addr`, and `kimage_voffset`.
- Exact XOR, variable shift, unsigned `B.CS`, low-39-bit mask, add, and
  subtract instruction paths.
- Three direct caller xrefs.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- The hardware/kernel mapping categories behind the two arithmetic paths are
  not named beyond their imported values and literal predicate.
