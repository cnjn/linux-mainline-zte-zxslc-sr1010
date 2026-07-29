# 0x13b88 idm_wifi_int

## Status

- Status: complete
- Confidence: verified.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature: `int idm_wifi_int(int irq, void *dev_id)`.

## Semantics

The handler ignores generic IRQ arguments, masks the source represented by
`idm_info + 0x4`, dispatches CPU-net source 1, and returns `1`:

```c
idm_int_disable(idm_info.word_4);
cpu_net_int(1);
return 1;
```

`idm_cfg_int` registers it as `g_idm_irq[1]` under the label `"idm"`.

## Concurrency

Like the CPU handler, it masks before the `cpu_net_int` NAPI handoff and does
not directly acknowledge hardware. `cpu_net_int(1)` selects the second
416-byte `int_info` context for NAPI accounting/scheduling.

## Evidence

- Direct decompilation and 10-instruction function body at `0x13b88`.
- Registration xrefs from `idm_cfg_int @ 0x14d88`.
- Shared `idm_int_disable` and `cpu_net_int` behavior established from their
  direct decompilation.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- The hardware meaning of `idm_info + 0x4` and source-1 NAPI poll behavior
  remain unresolved.
