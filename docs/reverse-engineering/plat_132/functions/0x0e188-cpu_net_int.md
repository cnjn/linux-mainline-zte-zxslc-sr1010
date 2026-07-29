# 0x0e188 cpu_net_int

## Status

- Status: complete
- Confidence: verified direct control flow, context stride, raw accounting
  offsets, imported NAPI calls, and all four internal callers.
- Size: `0x64` bytes, 24 ARM64 instructions.
- Recovered signature: `void cpu_net_int(unsigned int source)`.

## Semantics

The function selects one of four contiguous NAPI contexts from `int_info`:

```text
context = (void *)&int_info + 0x1a0 * source
```

It increments `context + 0x18c`, then calls `napi_schedule_prep(context)`.
When preparation succeeds it calls `__napi_schedule(context)`; otherwise it
increments `context + 0x190`. The latter counter records an IRQ delivery whose
NAPI work was already scheduled rather than issuing a duplicate schedule.

No range check is present. The only in-module callers provide source values
`0`, `1`, `2`, and `3`, mapping respectively to `cpu_net_poll`,
`idm_net_poll`, `cpu_idm_poll`, and `cpu_rls_poll` contexts established by
`cpu_net_init`.

IDA exposes the result of the final increment in `x0` on the already-scheduled
path, but callers ignore it and the scheduled path returns the residual result
of `__napi_schedule`; the semantic function return type is `void`.

## Concurrency

The two accounting increments are ordinary load/add/store sequences, with no
local atomic operation or lock. NAPI state arbitration is delegated to
`napi_schedule_prep`; this function does not acknowledge or re-enable the IDM
source, which was masked by the hard IRQ handler before this handoff.

## Evidence

- Complete 24-instruction ARM64 disassembly at `0xe188` through `0xe1e8`.
- Imported calls to `napi_schedule_prep` and `__napi_schedule`.
- `MADD` context calculation with literal stride `0x1a0`.
- Raw field accesses at offsets `0x18c` and `0x190`.
- All callers: `idm_cpu_int`, `idm_wifi_int`, `idm_all_int`, and `idm_rls_int`.
- Global layout: `int_info` starts at `0x27ab0`; the next global begins at
  `0x28130`, confirming four `0x1a0`-byte slots.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Exact vendor names and consumers of the two per-context counters are unknown.
- Poll functions must establish when each source is unmasked after NAPI
  completion.
