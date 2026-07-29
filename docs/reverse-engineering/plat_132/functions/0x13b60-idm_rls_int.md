# 0x13b60 idm_rls_int

## Status

- Status: complete
- Confidence: verified.
- Size: `0x28` bytes, 10 ARM64 instructions.
- Recovered signature: `int idm_rls_int(int irq, void *dev_id)`.

## Semantics

The handler ignores generic IRQ arguments, masks the source represented by
`idm_info + 0xc`, dispatches CPU-net source 3, and returns `1`:

```c
idm_int_disable(idm_info.word_c);
cpu_net_int(3);
return 1;
```

`idm_cfg_int` registers it as `g_idm_irq[2]` with label `"buf_rls"`.

## Concurrency

This is the source-3 instance of the IDM mask-before-NAPI pattern. It does not
read or acknowledge an IDM MMIO status register itself; `idm_int_disable` owns
the mask write and `cpu_net_int(3)` selects the fourth `int_info` context.

## Evidence

- Direct decompilation and 10-instruction function body at `0x13b60`.
- Registration xrefs from `idm_cfg_int @ 0x14d88`.
- Shared disable helper and `cpu_net_int` behavior established by direct
  decompilation.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- The hardware meaning of `idm_info + 0xc` and source-3 NAPI poll behavior
  remain unresolved.
