# 0x12bc zx_pon_int

## Status

- Status: complete
- Confidence: verified for control flow, status bits, calls, and MMIO offsets.
- Size: `0x338` bytes, 191 ARM64 instructions.
- Recovered signature: `int zx_pon_int(int irq, void *dev_id)`.

## Role

This is the top-level PON hard-IRQ dispatcher registered by
`register_pon_int`. It computes active PON interrupts from the status and mask
registers, dispatches protocol callbacks, maintains registration state, and
handles the DGi recovery sequence. It always returns `1`.

## Active Status and Shared Context

The first operation is:

```c
active_status = *(u32 *)(pon_base + 0x40) &
                ~*(u32 *)(pon_base + 0x44);
```

`pon_int_enable` proves that the `+0x44` register is the interrupt mask:
registration clears requested bits there. This handler does not directly
acknowledge or write either register.

The IRQ is registered with `&pon_int_info` as its `dev_id`. Every
`register_*_int` helper stores the supplied callback context in `pon_int_info`.
The dispatcher dereferences the supplied `dev_id` on every callback invocation,
so callbacks receive the current shared context-slot value. It increments the
adjacent 32-bit data at `dev_id + 0x18` after each callback dispatch; this is
the unnamed data location at `0x27780` when called through the top-level PON
registration.

## Bit Dispatch

The labels below are semantic names derived from the registered callback and
must not be treated as original vendor register-field names.

| Active bit | Behavior |
| --- | --- |
| `0x001` | If non-null, call `gpon_isr(ZX_INT_PON, context)`, increment the shared dispatch counter, then set `pon_registered` when GPON ONU state equals 5. |
| `0x010` | Set `rog_onu_flag = 1`; no callback or dispatch-counter increment. |
| `0x020` | Run the DGi recovery path described below. `dg_isr`, despite being storable through `register_dg_int`, is not called here. |
| `0x040` | If non-null, call `lp_isr`, then increment the counter. |
| `0x080` | If non-null, call `xgpon_isr`, increment the counter, then set `pon_registered` when XGPON ONU state equals 5. |
| `0x100` | If non-null, call `epon_isr`, increment the counter, and update `pon_registered` from EPON LLID state only when work-mode bit `0x20` is set. |
| `0x200` | If non-null, call `xeupon_isr`, increment the counter, and update `pon_registered` from XEPON LLID state only when work-mode mask `0x180` is nonzero. |
| `0x400` | If non-null, call `xedpon_isr`, then increment the counter. |
| `0x800` | Call `low_power_isr` without a null check, then increment the counter. A low-power interrupt therefore requires that callback to have been installed. |

All eligible bits are processed independently in the binary's fixed order;
there is no `else-if` short circuiting.

## DGi Recovery

For active bit `0x20`, the function always logs `"DGi\n"`. Before that log it
may run one of two guarded recovery sequences. Both invoke the exact fixed-loop
delay constants `0x8312b0` three times and `0x418958` once, call
`hw_power_optx_set(0)`, schedule `dg_timer_init`, and set `dg_flag = 1`.

- With `(g_pon_work_mode & 0x1a0) != 0` and clear `dg_flag`, it first calls
  `epon_set_dg_cnt`, then clears bit 0 at `pon_base + 0x180000` when mask
  `0xa0` is present and at `pon_base + 0x1c0004` when bit `0x100` is present.
- With `(g_pon_work_mode & 0x640) != 0` and clear `dg_flag`, it clears bits 0
  and 3 at `pon_base + 0x84000` when bit `0x40` is present, and clears bit 0 at
  `pon_base + 0x58400` when mask `0x600` is present.

The two guards are sequential, not mutually exclusive. Since a successful
first sequence sets `dg_flag`, it suppresses the second one during that IRQ.
`dg_timer_func @ 0x1154` later calls `hw_power_optx_set(1)`, restores the
affected mode-specific bits, and clears `dg_flag`.

## Registration and Ownership Evidence

- `register_pon_int @ 0x16a0` is the only top-level registration site and
  installs this handler with `&pon_int_info`.
- `register_gmac_int`, `register_xgmac_int`, `register_emac_int`,
  `register_xeumac_int`, `register_xedmac_int`, `register_lp_int`, and
  `register_low_power_int` each assign a callback and the shared context slot,
  then unmask their corresponding PON bit through `pon_int_enable`.
- `register_dg_int` similarly unmasks bit `0x20`, but `dg_isr` has no dispatch
  xref in this module.
- `dg_timer_func @ 0x1154` is the only other user of `dg_flag` and establishes
  the recovery completion behavior.

## Evidence

- Full ARM64 disassembly at `0x12bc` through `0x15f0`.
- Direct decompilation of `pon_int_enable @ 0x035c`, callback registration
  helpers at `0x0fa8` through `0x1124`, state readers at `0x125c`, `0x1274`,
  `0x128c`, and `0x12a4`, plus `epon_set_dg_cnt @ 0x11fc` and
  `dg_timer_func @ 0x1154`.
- Xrefs establish that `zx_pon_int` is installed by `register_pon_int`,
  `low_power_isr` is dispatched only here, and `dg_isr` is never dispatched by
  this module.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_irq.c`.

## Open Questions

- The vendor names and hardware semantics of the status register at `+0x40`,
  most DGi-mode masks, and the four DGi MMIO locations remain unverified.
- Callback registration and IRQ execution are not visibly synchronized here;
  external callback lifetime and registration ordering still need companion
  module evidence.
