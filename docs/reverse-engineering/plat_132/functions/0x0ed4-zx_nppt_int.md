# 0x0ed4 zx_nppt_int

## Status

- Status: complete
- Confidence: verified for control flow, status bits, callbacks, and MMIO
  offsets.
- Size: `0xd4` bytes, 52 ARM64 instructions.
- Recovered signature: `int zx_nppt_int(int irq, void *dev_id)`.

## Role

This is the top-level NPPT hard-IRQ dispatcher installed by
`register_nppt_int`. It dispatches OAM, PTP, and PTP-stamp events and always
returns `1`.

## Active Status

The handler computes active events as:

```c
active_status = *(u32 *)(nppt_base + 0x0) &
                ~*(u32 *)(nppt_base + 0x4);
```

`nppt_int_enable @ 0x374` clears requested bits in `nppt_base + 0x4`, proving
that second location is the interrupt mask. This dispatcher does not directly
acknowledge or write either status/mask location.

## Bit Dispatch

The labels below are semantic names derived from their callback registration
helpers, not recovered vendor register-field names. The branches execute in the
binary's order: OAM, PTP, then PTP stamp.

| Active bit | Callback and side effects |
| --- | --- |
| `0x100` | If non-null, call `oam_isr(0, 0)`, set `soam_alarm_flag = 1`, then perform the 11 volatile MMIO reads listed below. |
| `0x400` | If non-null, call `ptp_isr(0, 0)`. |
| `0x200` | If non-null, call `ptp_stamp_isr(ZX_INT_PON, *(void **)dev_id)`. |

The callback ABI is intentionally asymmetric: OAM and PTP do not receive the
shared context, while PTP stamp receives the current value in the
`pon_int_info` slot through the top-level IRQ's `dev_id`. No branch updates the
PON dispatch counter used by `zx_pon_int`.

## OAM Post-Callback Reads

After a non-null OAM callback returns, the function reads and discards these
32-bit volatile locations relative to `nppt_base`:

```text
0x1c000  0x1c004  0x1c008  0x1c00c
0x1c2c4  0x1c2c8  0x1c2cc  0x1c2d0
0x1c2d4  0x1c2d8  0x1c2dc
```

The binary supplies no names or writes that establish whether these reads
acknowledge, drain, or merely sample OAM state, so the recovery preserves them
as exact volatile reads.

## Registration and Ownership Evidence

- `register_nppt_int @ 0x1710` is the sole top-level registration site and
  installs this handler with `&pon_int_info`.
- `register_oam_int @ 0x1134` assigns `oam_isr` and unmasks bit `0x100` through
  `nppt_int_enable`.
- `register_ptp_int @ 0x10e4` and `register_ptp_stamp_int @ 0x110c` assign
  their callback slots, replace the shared `pon_int_info` context value, and
  unmask bits `0x400` and `0x200`, respectively.
- `soam_alarm_flag` is written only by this handler within the module.

## Evidence

- Full ARM64 disassembly at `0x0ed4` through `0x0fa4`.
- Direct decompilation of `nppt_int_enable @ 0x0374`,
  `register_oam_int @ 0x1134`, `register_ptp_int @ 0x10e4`, and
  `register_ptp_stamp_int @ 0x110c`.
- Xrefs prove the top-level registration, the three callback slots, and the
  unique in-module write to `soam_alarm_flag`.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_irq.c`.

## Open Questions

- The NPPT status register field names and the purpose of the OAM post-callback
  reads need hardware documentation or companion-module evidence.
- The external consumers of `soam_alarm_flag` and the lifetime of callback
  slots are not established inside this module.
