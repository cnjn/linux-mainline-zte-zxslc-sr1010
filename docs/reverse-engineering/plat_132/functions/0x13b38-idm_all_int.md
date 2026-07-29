# 0x13b38 idm_all_int

## Status

- Status: complete
- Confidence: verified.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature: `int idm_all_int(int irq, void *dev_id)`.

## Semantics

The handler ignores generic IRQ arguments, masks the source represented by
`idm_info + 0x8`, dispatches CPU-net source 2, and returns `1`:

```c
idm_int_disable(idm_info.word_8);
cpu_net_int(2);
return 1;
```

`idm_cfg_int` registers it as `g_idm_irq[3]` under label `"localtest"`.

## Concurrency

This is the source-2 instance of the IDM mask-before-NAPI pattern. It performs
no direct hardware acknowledgement; the shared disable helper writes the mask,
and `cpu_net_int(2)` selects the third `int_info` NAPI context.

## Evidence

- Direct decompilation and 10-instruction function body at `0x13b38`.
- Registration xrefs from `idm_cfg_int @ 0x14d88`.
- Shared disable helper and `cpu_net_int` behavior established by direct
  decompilation.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- The hardware meaning of `idm_info + 0x8` and source-2 NAPI poll behavior
  remain unresolved.
