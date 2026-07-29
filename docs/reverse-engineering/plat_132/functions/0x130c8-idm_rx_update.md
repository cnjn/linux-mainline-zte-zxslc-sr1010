# 0x130c8 idm_rx_update

## Status

- Status: complete
- Confidence: verified ordering, three-word ABI, both MMIO writes, and ops-table
  callback role.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature:
  `void idm_rx_update(u32 queue, u32 count, u32 jumbo_count)`.

## Semantics

Issues `DSB ST`, then writes `count | (queue << 12)` to IDM `+0x088` and
`(count - jumbo_count) | (jumbo_count << 16)` to IDM `+0x100`. Inputs are not
masked or range checked. `W0` happens to retain the first packed value at
return, but the installed ops-table callback ABI discards it.

## Caller Context

`idm_init` installs this entry at `idm_ops + 0x48`; CPU RX paths invoke that
slot after flushing refill staging. There are no direct BL callers in this
module.

## Evidence

- Complete ARM64 body at `0x130c8` through `0x130ec`.
- Ops-table pointer at `0x26720` and the recovered `cpu_net_ops` layout.
- `cpu_net_rx` and `idm_net_rx` pass queue index, processed count, and observed
  jumbo count through the `+0x48` slot.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- Hardware meanings of the two packed register fields remain unlabelled beyond
  their queue/count calling context.
