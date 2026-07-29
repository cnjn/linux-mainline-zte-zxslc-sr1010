# 0x0afc0 regisetr_low_power_send_pkt_handle

## Status

- Status: complete
- Confidence: verified pointer store, residual return, export, and external
  import boundary.
- Size: `0x0c` bytes, 3 ARM64 instructions.
- Recovered signature:
  `zte_low_power_send_t regisetr_low_power_send_pkt_handle(zte_low_power_send_t callback)`.

## Semantics

Stores the supplied five-argument callback pointer in `low_power_send` and
returns the same pointer unchanged. It accepts null, so callers can clear the
callback slot. There is no validation, synchronization, reference management,
or prior-callback return behavior.

## Caller Context

There are no direct in-module callers. The function is exported as
`__ksymtab_regisetr_low_power_send_pkt_handle`; `np.ko` imports it and also
imports the paired judge registration API. `cpu_lowpower_tx @ 0x0d49c` reads
the resulting slot before invoking it.

## Concurrency and Ownership

- Plain unsynchronized pointer publication.
- The provider retains callback lifetime responsibility; this module takes no
  ownership or reference.

## Evidence

- Complete three-instruction ARM64 body at `0xafc0` through `0xafc8`.
- No direct IDA xrefs; runtime kallsyms export and `np.ko` undefined-symbol
  evidence establish external use.
- Consumer cross-check in `cpu_lowpower_tx`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Callback provider lifecycle and exact semantics of its five arguments.
