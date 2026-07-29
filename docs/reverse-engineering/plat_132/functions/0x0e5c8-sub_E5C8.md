# 0x0e5c8 sub_E5C8

## Status

- Status: complete
- Confidence: verified raw instruction sequence, fall-through result, and zero
  direct xrefs; purpose remains unknown.
- Size: `0x18` bytes, 6 ARM64 instructions plus fall-through into
  `__fswab32 @ 0x0e5e0`.
- Recovered signature: `u32 sub_E5C8(u64 priority_mask)`.

## Semantics

The helper writes its 64-bit input to `ICC_PMR_EL1`, issues `DSB SY`, reads the
register back, and writes the low 32 bits XORed with `0xe0` to `ICC_PMR_EL1`.
It then reads `TPIDR_EL2` with no architectural data use.

There is no `RET` before the adjacent byte-swap entry. Execution falls through
to `REV W0,W0`, so the observed return is the byte-swapped low 32 bits of the
original `priority_mask`; the later PMR readback and TPIDR_EL2 read do not alter
`w0`.

Unlike the similar `sub_10738`, this function writes the supplied PMR value
first and leaves the XORed readback programmed. Its purpose must therefore
remain unknown rather than being labeled an IRQ save/restore primitive.

## Concurrency and Ownership

Touches CPU-local interrupt-controller and thread-pointer system registers. It
has no module globals, MMIO, allocation, callback, lock, or ownership transfer.

## Evidence

- Complete instructions at `0x0e5c8` through `0x0e5dc`.
- `MSR ICC_PMR_EL1,X0`, `DSB SY`, readback/XOR/write sequence, and discarded
  `MRS TPIDR_EL2`.
- Function boundary ends at `0x0e5e0`, proving direct fall-through into the
  verified byte-swap entry.
- Zero direct IDA xrefs to the entry.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Why this unreferenced fragment leaves the PMR XORed after its readback.
- Whether it is externally reachable or retained dead code.
