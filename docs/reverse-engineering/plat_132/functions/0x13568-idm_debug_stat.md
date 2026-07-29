# 0x13568 idm_debug_stat

## Status

- Status: complete
- Confidence: verified all software-counter indexing, fixed two-CPU loops,
  output order, repeat-counter locations, `idm_stat` call, and exported
  interface.
- Size: `0x154` bytes, 81 ARM64 instructions.
- Recovered signature: `void idm_debug_stat(void)`.

## Semantics

Prints normal and jumbo buffer-pool software counters for exactly CPU indices
zero and one, then prints two repeat counters and invokes `idm_stat`. The
32-bit `idm_status` layout used by this diagnostic is:

| Pool | CPU `n` entries |
| --- | --- |
| normal | `2*n + {0, 4, 8, 12}` for `free_cnt`, `alloc_cnt`, `free_cnt1`, `alloc_cnt1` |
| jumbo | `2*n + {1, 5, 9, 13}` for the same four labels |
| repeat | indices 16 (`free_repeat`) and 17 (`alloc_repeat`) |

The routine neither locks nor snapshots the counters. It does not return an
evidenced status; the `W0` value left by the final `idm_stat` call is residual.

## Caller Context

There are no direct module code callers. `__ksymtab_idm_debug_stat @ 0x1c768`
exports the diagnostic API. It is the only direct module caller of `idm_stat`.

## Evidence

- Complete 81-instruction ARM64 body at `0x13568` through `0x136b8`.
- Exact 32-bit load offsets from `idm_status @ 0x28ce8`.
- Direct `BL idm_stat` at `0x1369c`.
- Export entry `__ksymtab_idm_debug_stat @ 0x1c768`.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- The ownership and reset policy for each software counter are not established
  by this read-only diagnostic path.
