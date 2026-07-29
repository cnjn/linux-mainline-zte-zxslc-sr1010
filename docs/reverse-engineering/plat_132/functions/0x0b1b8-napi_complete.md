# 0x0b1b8 napi_complete

## Status

- Status: complete
- Confidence: verified fixed forwarding call, argument value, imported target,
  all direct callers, and semantically unused residual return register.
- Size: `0x18` bytes, 6 ARM64 instructions.
- Recovered signature: `void napi_complete(zte_napi_t *napi)`.

## Semantics

Thin local wrapper around the imported NAPI completion helper:

```c
napi_complete_done(napi, 0);
```

The supplied NAPI pointer remains in `x0`; the wrapper writes literal zero to
`w1` before calling `napi_complete_done`. It has no local state, lock, branch,
or validation. The imported helper's residual return value is ignored by every
direct caller, so this wrapper is represented as void.

## Caller Context

The four direct callers are the completion paths in `cpu_net_poll @ 0x0cce4`,
`cpu_idm_poll @ 0x0cb20`, `idm_net_poll @ 0x0c294`, and
`cpu_rls_poll @ 0x0b86c`. Each follows the wrapper with the matching IDM-source
unmask operation, except the release poll follows its fixed release-processing
path.

## Concurrency and Ownership

- No local synchronization, allocation, ownership transfer, or global state.
- NAPI state and import-level synchronization semantics belong to
  `napi_complete_done` and its callers.

## Evidence

- Complete six-instruction ARM64 body at `0xb1b8` through `0xb1cc`.
- Direct `BL napi_complete_done` import at `0xb1c4`; literal second argument is
  zero at `0xb1bc`.
- Four direct IDA callers and runtime kallsyms evidence for the imported helper
  and module-local wrapper.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_cpu_net.c`.

## Open Questions

- Vendor-kernel meaning of a zero `work_done` argument in this wrapper's NAPI
  lifecycle.
