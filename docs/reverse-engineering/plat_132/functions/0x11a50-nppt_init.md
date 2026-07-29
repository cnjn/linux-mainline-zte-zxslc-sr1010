# 0x11a50 nppt_init

## Status

- Status: complete
- Confidence: verified
- Size: `0x5c` bytes, 22 ARM64 instructions.

## Recovered Signature

```c
int nppt_init(void);
```

## Semantics

The function always invokes all four initialization stages in this exact order:

1. `sipc_init`
2. `greg_init`
3. `nppt_smac_init`
4. `idm_init`

It computes and returns the bitwise OR of their 32-bit status values. There is
no short-circuit and no cleanup when an earlier stage returns an error.

The exact assembly sequence is `ORR W19, W19, W0`, then after IDM initialization
`ORR W0, W20, W0; ORR W19, W0, W19`. This proves the aggregation is bitwise OR,
not logical OR or first-error propagation.

## Known Callee Facts

- `sipc_init @ 0x165b8` writes zero to `nppt_base + 0x4000` and returns 0.
- `greg_init @ 0x11f00` programs NPPT buffer-size-related registers, masks
  SMAC runt errors, and returns 0.
- `nppt_smac_init @ 0x129c8` initializes SMAC/PHY/XMAC state and always returns
  zero after starting its PHY worker.
- `idm_init @ 0x14ff4` is the later memory, descriptor-ring, IRQ, and CPU
  netdev initialization target.

The module-exit counterpart `nppt_exit @ 0x11bcc` calls `idm_exit` first and
then `smac_del_phy_scan`. It is only reached through `plat_cleanupModule`.

## Error Behavior

- `nppt_smac_init` cannot contribute a nonzero status despite ignoring its own
  child helper statuses; IDM initialization still runs after it.
- The function logs the final OR-combined result and returns it unchanged.
- No rollback appears in this function.

## Runtime Evidence

The vendor runtime logs `nppt init start`, the SMAC initialization sequence,
IDM buffer/ring setup, and `nppt init end. ret = 0` in one contiguous boot
sequence. This corroborates the observed call order.

## Source-Like Reconstruction

The reconstructed function is appended to
`docs/reverse-engineering/plat_132/recovered/plat_module.c`.

## Open Questions

- Determine which nonzero status values `idm_init` can return and whether the
  bitwise OR can obscure individual IDM failures.
- Reconstruct `nppt_exit`, `idm_exit`, and `smac_del_phy_scan` before modeling
  a complete init-failure unwind policy.
