# 0x0af74 regisetr_omci_mic_add_handle

## Status

- Status: complete
- Confidence: verified pointer store, residual return, export, and consumer.
- Size: `0x0c` bytes, 3 ARM64 instructions.
- Recovered signature:
  `zte_omci_mic_handler_t regisetr_omci_mic_add_handle(zte_omci_mic_handler_t callback)`.

## Semantics

Stores the supplied data/length callback in `omci_mic_add` and returns the same
pointer. Null is accepted and clears the slot. It has no validation,

## Caller Context

There are no direct in-module callers. `cpu_net_tx @ 0x0d668` invokes the slot
for management TX only when work-mode bits `0x600` are set and the slot is
non-null; nonzero callback status drops the skb. The registration API is exported,
undefined-symbol import for it.

## Concurrency and Ownership

- Plain unsynchronized callback store.
- Callback provider owns lifetime and must coordinate with the TX consumer.

## Evidence

- Complete three-instruction ARM64 body at `0xaf74` through `0xaf7c`.
- No direct IDA xrefs; runtime kallsyms export and `cpu_net_tx` consumer xref.
- Companion-module undefined-symbol checks found no observed importer in the
  captured module set named above.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Callback provider/lifetime and exact MIC-add status/error contract.
