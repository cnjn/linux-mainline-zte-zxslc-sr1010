# 0x139a4 set_idm_int_cpu_rx_cpu_config

## Status

- Status: complete
- Confidence: verified two-CPU gate, exact selector behavior, affinity-mask
  addresses, global stores, ignored affinity return, and exported-interface
  status.
- Size: `0x88` bytes, 34 ARM64 instructions.
- Recovered signature:
  `void set_idm_int_cpu_rx_cpu_config(u32 target_cpu)`.

## Semantics

Does nothing unless `nr_cpu_ids == 2`. For `target_cpu == 1`, assigns CPU IRQ
zero the CPU1 affinity-mask entry, writes one to both `g_idm_irq_to_cpu` and
`eth_xmit_mode`. Every other input selects CPU0, writes zero to both globals,
and assigns CPU0's affinity-mask entry. The `irq_set_affinity_hint` status is
discarded.

The two mask pointers occur at addresses IDA labels `uNPPT_IDM_RX_QUEUE_NUM`
and `uIDM_TX_JUMBO_BP_RETRV_NUM`, but they are `cpu_bit_bitmap + 1` and
`cpu_bit_bitmap + 2` respectively. This matches the CPU0/CPU1 mask addressing
already verified in `idm_cfg_int`; the neighboring imported-variable labels are
not evidence that queue-count values are passed as masks.

## Caller Context

There are no direct module code callers. `__ksymtab_set_idm_int_cpu_rx_cpu_config
@ 0x1ccf0` exports the control API.

## Evidence

- Complete ARM64 body at `0x139a4` through `0x13a28`.
- `nr_cpu_ids == 2` gate and exact `target_cpu == 1` branch.
- CPU-mask address sequence at `0x139dc`/`0x139e4` and
  `0x139fc`/`0x13a04`.
- Existing `idm_cfg_int` affinity calls through the same `cpu_bit_bitmap` lanes.

## Source-Like Reconstruction

`recovered/plat_idm.c`.

## Open Questions

- External users' policy for values other than zero/one is unknown; the binary
  unconditionally maps all such values to CPU0.
