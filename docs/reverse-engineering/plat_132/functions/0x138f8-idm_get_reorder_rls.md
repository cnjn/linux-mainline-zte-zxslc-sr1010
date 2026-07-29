# 0x138f8 idm_get_reorder_rls

## Status

- Status: complete
- Confidence: verified all queue cases, exact CPU-variant gate, indirect ops
  registration, return width, and lack of synchronization.
- Size: `0x84` bytes, 33 ARM64 instructions.
- Recovered signature: `uint32_t idm_get_reorder_rls(unsigned int queue)`.

## Semantics

Reads raw IDM reorder-release counts by queue selector:

| Queue | Condition | IDM offset | Result |
| --- | --- | --- | --- |
| 0 | always | `0x08c` | raw 32-bit word |
| 1 | always | `0x0f8` | raw 32-bit word |
| 2 | CPU-133 or CPU-129 predicate returns exactly 1 | `0x41c` | raw 32-bit word |
| other | always | none | zero |

Queue two returns zero on every other CPU type. The function performs no range
check beyond the explicit selector comparisons, no lock/barrier, and no stable
hardware snapshot.

## Caller Context

The function is local text with no direct `BL` callers. Its address is stored in
the IDM operations table at `0x26730` (`+0x58`), then reached indirectly by
`net_check_reorder_rls_nolock @ 0x0b050`. That caller clamps the raw result to
`0xfff` before dispatching recycle callbacks. `idm_rls_update @ 0x13740` is the
paired `+0x60` table operation.

## Evidence

- Complete ARM64 body at `0x138f8` through `0x13978`.
- Exact fixed loads at `0x13908`, `0x13924`, and `0x13954`.
- Queue-two comparisons and CPU predicates at `0x1392c` through `0x13968`.
- Data xref at `0x26730` identifies the IDM ops-table registration.
- Parent release-reclaim record confirms the indirect call path and paired
  update operation.

## Source-Like Reconstruction

`recovered/plat_idm.c`.
