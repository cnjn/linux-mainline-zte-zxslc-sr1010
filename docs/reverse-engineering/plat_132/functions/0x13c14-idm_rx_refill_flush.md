# 0x13c14 idm_rx_refill_flush

## Status

- Status: complete
- Confidence: verified current-CPU lookup, staging layout, lock/release order,
  ring copy loops, count resets, ring-index updates, and ops-table role.
- Size: `0x138` bytes, 76 ARM64 instructions.
- Recovered signature: `void idm_rx_refill_flush(void)`.

## Semantics

Reads the current CPU number through `cpu_number + __my_cpu_offset_0()`, locks
`idm_refill_lock`, and drains that CPU's two 32-entry staging arrays into the
normal and jumbo RX rings. Staging has this verified 264-byte layout:

| Offset | Contents |
| --- | --- |
| `0x000..0x07c` | 32 normal staged words |
| `0x080..0x0fc` | 32 jumbo staged words |
| `0x100` | normal staged count |
| `0x104` | jumbo staged count |

Normal entries advance `idm_refill_index` and wrap against
`uIDM_RX_NORMAL_BP_NUM`; the index is written back only if at least one normal
entry was copied. The function clears normal count before draining jumbo entries.
Jumbo entries advance the source-like `idm_jumbo_refill_index` alias at binary
address `0x28d6c`, but notably also wrap against `uIDM_RX_NORMAL_BP_NUM`, not
with `STLRB` semantics.

## Caller Context

Called directly by `idm_rx_refill0 @ 0x14144` when a per-pool staged count
exceeds 31, and installed in the IDM ops table at `0x26710` (`+0x38`).

## Evidence

- Complete ARM64 body at `0x13c14` through `0x13d48`.
- Per-CPU `0x108` byte stride (`66` 32-bit words).
- Ring-copy loops, count stores, and release `STLRB` at `0x13d38`.
- Direct caller at `0x14298` and ops-table pointer at `0x26710`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- The jumbo drain's normal-pool wrap bound may be a vendor bug or an implicit
  equal-size constraint; it is preserved verbatim.
