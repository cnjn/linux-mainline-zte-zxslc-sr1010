# 0x13bb0 idm_cpu_int

## Status

- Status: complete
- Confidence: verified.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature: `int idm_cpu_int(int irq, void *dev_id)`.

## Semantics

The top-level IDM CPU IRQ handler ignores both generic IRQ arguments, masks its
source using the first `idm_info` word, hands source index zero to `cpu_net_int`,
and returns `1`:

```c
idm_int_disable(idm_info.word_0);
cpu_net_int(0);
return 1;
```

`idm_cfg_int` registers it as IRQ `g_idm_irq[0]` with the label `"cpu"`.

## Deferred Processing Handoff

`cpu_net_int @ 0xe188` selects `int_info + 416 * source`. For source zero it
increments that NAPI context's interrupt counter and either schedules it through
`_napi_schedule` or increments its already-scheduled counter. Thus this handler
masks before scheduling NAPI rather than polling or acknowledging hardware
directly.

## Evidence

- Full 10-instruction ARM64 disassembly at `0x13bb0` through `0x13bd4`.
- Sole registration xrefs from `idm_cfg_int @ 0x14d88`.
- Direct decompilation of `cpu_net_int @ 0xe188` for the source-zero NAPI
  handoff.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- The exact NAPI context layout and the re-enable operation after poll
  completion require reconstruction of `cpu_net_int` and its poll paths.
