# 0x0af84 register_omci_mic_check_handle

## Status

- Status: complete
- Confidence: verified pointer store, residual return, export, and consumer.
- Size: `0x0c` bytes, 3 ARM64 instructions.
- Recovered signature:
  `zte_omci_mic_handler_t register_omci_mic_check_handle(zte_omci_mic_handler_t callback)`.

## Semantics

Stores the supplied data/length validation callback in `omci_mic_check` and
returns that same pointer. Null clears the slot. It has no validation,
previous-callback return behavior.

## Caller Context

There are no direct in-module callers. `cpu_omci_rx @ 0x0c4b0` tests and invokes
exists in the collected `np.ko`, `switch.ko`, or `peripheral.ko` module set.

## Concurrency and Ownership

- Plain unsynchronized callback store.
- Callback provider owns lifetime and must coordinate with RX execution.

## Evidence

- Complete three-instruction ARM64 body at `0xaf84` through `0xaf8c`.
- No direct IDA xrefs; runtime kallsyms export and CPU-OMCI-RX consumer xref.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Callback provider/lifetime and exact validation status contract.
