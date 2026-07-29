# 0x0e1ec cpu_register_netinfo

## Status

- Status: complete
- Confidence: verified copy offsets and return behavior; destination labels are
  analyst names.
- Size: `0x34` bytes, 13 ARM64 instructions.
- Recovered signature: `int cpu_register_netinfo(struct idm_net_info *info)`.

## Semantics

The function copies the five fields of a 24-byte input record into independent
CPU-net globals:

```c
cpu_net_ops         = *(void **)(info + 0x10);
cpu_net_info_word_0 = *(u32 *)(info + 0x0);
cpu_net_info_word_4 = *(u32 *)(info + 0x4);
cpu_net_info_word_8 = *(u32 *)(info + 0x8);
cpu_net_info_word_c = *(u32 *)(info + 0xc);
return *(u32 *)(info + 0xc);
```

The four 32-bit destinations are not contiguous in module data, so the source
reconstruction intentionally keeps them as separate analyst-labeled globals.

## Call Context

`idm_init` calls this immediately after successful `idm_cfg_int`, passing the
`idm_info` record it initialized with words `65023`, `0x00ff0000`, and `512`,
plus the `idm_ops` pointer. `idm_init` ignores this function's return value.

## Evidence

- Full 13-instruction ARM64 disassembly at `0x0e1ec` through `0x0e21c`.
- Direct caller evidence from `idm_init @ 0x14ff4`.
- Exact input offsets and destination globals in IDA xrefs.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_idm.c`.

## Open Questions

- The original type name and semantics of all five fields, particularly the
  ops-table ABI, remain unresolved.
