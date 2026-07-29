# 0x0af68 register_omci_oam_handle

## Status

- Status: complete
- Confidence: verified pointer store, residual return, exports, consumer, and
  companion-module global import boundary.
- Size: `0x0c` bytes, 3 ARM64 instructions.
- Recovered signature:
  `zte_omci_oam_rx_t register_omci_oam_handle(zte_omci_oam_rx_t callback)`.

## Semantics

Stores the supplied OMCI/OAM receive callback in `omci_oam_rx` and returns the
same pointer. Null is accepted, clearing the callback. It provides no validation,
synchronization, lifetime tracking, or old-callback return.

## Caller Context

There are no direct in-module callers. `cpu_omci_rx @ 0x0c4b0` consumes the slot
for received management traffic. Both `register_omci_oam_handle` and the mutable
`omci_oam_rx` slot are exported; runtime evidence shows `np.ko` imports the slot
directly rather than this registration function.

## Concurrency and Ownership

- Plain unsynchronized function-pointer write.
- Callback provider owns lifetime and synchronization.
- Direct external access to the exported slot has the same race surface as this
  API.

## Evidence

- Complete three-instruction ARM64 body at `0xaf68` through `0xaf70`.
- No direct IDA xrefs; runtime kallsyms establish both exports.
- `np.ko` undefined-symbol evidence for `omci_oam_rx` and consumer cross-check
  in `cpu_omci_rx`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Callback provider/lifetime and why the slot is separately exported.
