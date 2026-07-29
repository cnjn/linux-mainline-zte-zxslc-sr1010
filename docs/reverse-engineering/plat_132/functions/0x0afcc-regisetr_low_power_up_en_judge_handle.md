# 0x0afcc regisetr_low_power_up_en_judge_handle

## Status

- Status: complete
- Confidence: verified pointer store, residual return, export, and external
  import boundary.
- Size: `0x0c` bytes, 3 ARM64 instructions.
- Recovered signature:
  `zte_low_power_up_en_judge_t regisetr_low_power_up_en_judge_handle(zte_low_power_up_en_judge_t callback)`.

## Semantics

Stores the supplied no-argument judge callback in `low_power_up_en_judge` and
returns that same pointer unchanged. Null is accepted and clears the slot. The
function does not validate the callback, synchronize publication, retain a
reference, or return the former callback.

## Caller Context

There are no direct in-module callers. The function is exported as
`__ksymtab_regisetr_low_power_up_en_judge_handle`; `np.ko` imports it together
with the paired `low_power_send` registration function. `cpu_lowpower_tx @
0x0d49c` calls the published callback one to three times per direct-TX attempt.

## Concurrency and Ownership

- Plain unsynchronized pointer publication.
- Callback provider owns lifetime and any necessary synchronization.

## Evidence

- Complete three-instruction ARM64 body at `0xafcc` through `0xafd4`.
- No direct IDA xrefs; runtime kallsyms export and `np.ko` undefined-symbol
  evidence establish external use.
- Consumer cross-check in `cpu_lowpower_tx`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Provider lifecycle and the exact meanings of the judge return values.
