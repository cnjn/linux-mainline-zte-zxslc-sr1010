# 0x10778 lower_net_smb_test_config

## Status

- Status: complete
- Confidence: verified branch behavior, global writes, affinity-hint argument
  arithmetic, 16-bit threshold store, return values, and callback publication.
- Size: `0x90` bytes, 33 ARM64 instructions.
- Recovered signature: `int lower_net_smb_test_config(int value)`.

## Semantics

For nonzero `value`, the function sets `net_gro_en` to 3, logs the start
message, stores 1 to the low 16 bits of `g_net_check_threshold`, and returns 1.

For zero `value`, it logs the end message, clears `net_gro_en` and
`net_smb_state`, calls `irq_set_affinity_hint(g_idm_irq[0], mask)`, stores 2 to
the low 16 bits of `g_net_check_threshold`, and returns 2. The mask address is
computed from `cpu_bit_bitmap` and `g_idm_irq_to_cpu` with the raw ARM64
expression:

```c
(u8 *)cpu_bit_bitmap + 8 * (cpu & 0x3f) + 8 - 8 * (cpu >> 6)
```

The affinity-hint return value is ignored.

## Callback Context

`net_gro_init @ 0x1150c` stores this function in `pp_smb_test_config`; that
data-store is the only in-module xref. Its actual invocation is therefore an
external callback boundary and is not inferred from the assignment alone.

## Concurrency and Ownership

- No local lock, allocation, ownership transfer, or direct MMIO access.
- The callback mutates shared GRO globals and an IRQ affinity hint without a
  visible serialization mechanism.

## Evidence

- Complete 33-instruction ARM64 disassembly at `0x10778` through `0x10804`.
- Direct callback-pointer publication in `net_gro_init`.
- Raw branch, global-store, CPU-mask arithmetic, and import argument setup.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- External invoker and meaning of nonzero callback values.
- Exact type/layout of the CPU bitmap and intended long-term affinity policy.
