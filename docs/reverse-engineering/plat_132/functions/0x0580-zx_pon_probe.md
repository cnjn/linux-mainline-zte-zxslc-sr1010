# 0x0580 zx_pon_probe

## Status

- Status: complete
- Confidence: strong inference
- Size: `0x84c` bytes.
- Candidate signature: `int zx_pon_probe(struct platform_device *pdev)`.

The argument type is an upstream Linux 5.4.196 platform-driver ABI reference.
The vendor binary proves only that the first argument is dereferenced as a
device-name pointer for logging; no unmodified upstream structure layout is
assumed beyond that candidate ABI.

## Role

This is the platform bring-up function for the vendor PON/NPPT data plane. It
enumerates every OF node matching `zx_pon_match`, maps hardware resources,
collects IRQs, selects a PON mode, performs reset/clock/SerDes preparation, and
registers the PON and NPPT top-level IRQ handlers.

## Resource Discovery

Only CPU types 133 and 129 execute the OF-resource loop. The confirmed vendor
compatibles and resulting global state are:

| Compatible | Resource index or IRQ index | Result |
| --- | --- | --- |
| `zte,zx279133-pon` | `reg[0..4]` | `pon_base`, `sys_ctrl_base`, `top_crm_base`, `pin_mux_base`, `efuse_base` |
| `zte,zx279133-pon` | `irq[0..4]` | PON plus WOE0/WOE1 TX/RX IRQ globals |
| `zte,zx279133-pps` | `reg[0]`, `irq[0]` | `pps_base`, `g_pps_irq` |
| `zte,zx279133-nppt` | `reg[0]`, `irq[0]` | `nppt_base`, `g_nppt_irq` |
| `zte,zx279133-rgmii` | `reg[0]` | `rgmii_base` |
| `zte,zx279133-idm-intr` | `irq[0..3]` | `g_idm_irq[0..3]` |
| `zte,zx279133-xmac0-pcs` | `reg[0]` | `xmac0_pcs_base` |
| `zte,zx279133-pon_serdes` | `reg[0]` | `pon_serdes_base` |
| `zte,zx279133-pon_serdes_pll` | `reg[0]` | `pon_serdes_pll_base` |
| `zte,zx279133-uni_serdes` | `reg[0]` | `uni_serdes_base` |
| `zte,zx279133-pcu` | `reg[0]` | `pcu_base` |
| `zte,zx279133-gephy-apb` | `reg[0]` | `gephy_apb_base` |

The vendor DTS provides the corresponding physical resources. Runtime logs
confirm that all relevant mappings were present and that IDM IRQs resolved to
26 through 29. The static DTS is vendor evidence; the upstream Linux tree was
not used to derive these resources.

## Low-Power Alias Mapping

Inside the same OF-node loop, the function manually maps 0x200-byte aliases at
these physical addresses:

| Global | Physical address | Relationship to vendor DTS |
| --- | --- | --- |
| `lowpower_config_base` | `0x10e10000` | top CRM resource |
| `lowpower_xmac_config_base` | `0x16100000` | UNI SerDes resource |
| `lowpower_gpio_config_base` | `0x10e20000` | pinmux resource |

The original code chooses one of two ARM64 ioremap flag values according to
`arm64_kernel_use_ng_mappings`. It does not check these three manual mapping
results. Because this code is outside each compatible-specific branch but
inside the enumeration loop, it executes for every matching node; do not move
it outside that loop in a semantic reconstruction.

## Initialization Sequence

1. Call `CspGetPortInfo`; on success, copy the returned 16-bit mode into both
   `g_pon_work_mode` and `g_pon_work_mode_orignal`.
2. On CPU 133 or 129, enable IDM CCI and initialize PON CCI, WOE, core, TM,
   and NPPT clocks. Their individual return values are not tested here.
3. Assert SDET reset, delay, issue `pon_sys_soft_reset`, invoke `sipc_init`,
   and poll `greg_init_done_check`.
4. If initialization succeeds, restore SDET and select clock/reset/SerDes mode
   from the first matching bit in `g_pon_work_mode`.
5. On CPU 133 or 129, correct `lan_up_port` from `get_eth_wan_port() + 1`, set
   the PON-SerDes-to-XMAC1 route when appropriate, and update an unknown
   sysctrl field from efuse bits 26 through 29.
6. Register PON IRQ, then NPPT IRQ. Both must succeed for probe to return 0.

## PON Mode Mapping

| Work-mode bit | Logged mode | `zx_pon_clk_reset_init` argument |
| --- | --- | --- |
| `0x040` | GPON | 5 |
| `0x200` | XGPON | 6 |
| `0x400` | XGPONS | 7 |
| `0x020` | EPON | 0 |
| `0x080` | XEPON | 1 |
| `0x100` | XEPONS | 4 |
| `0x010` | P2P | 15 |
| none above | `MODE_XGPON_SYN` fallback | 7 |

The branch order is significant when multiple bits are set. P2P additionally
sets `lan_up=1` and initially sets `lan_up_port=0`; the CPU-133/129 correction
can overwrite that port. The collected runtime ran P2P (`0x10`) and logged a
corrected port value of 6.

## Error Behavior

- Any failed required OF mapping, failed required PON/PPS/NPPT/IDM IRQ lookup,
  or failed required peripheral mapping returns the literal signed status
  `-19` after logging. Existing mappings are not unwound in this function.
- `greg_init_done_check` failure propagates unchanged.
- Negative returns from `register_pon_int` and `register_nppt_int` propagate
  unchanged. If NPPT IRQ registration fails after PON IRQ registration, this
  function does not free the PON IRQ.

## Evidence

- IDA decompilation at `0x0580` and xrefs to every mapped global.
- Vendor compatible strings in `.rodata`.
- Vendor static DTS: `vendor-reference/2b5/zx279133-sr1010.dts`.
- Vendor runtime log records resource mappings, mode selection, and successful
  PON/NPPT IRQ registration.
- `greg_init_done_check @ 0x11c64`, `pon_sys_soft_reset @ 0x0274`, and
  `zx_pon_clk_reset_init @ 0x8088` were separately decompiled to verify the
  reset-poll-SerDes sequence.

## Source-Like Reconstruction

The semantic C reconstruction is in
`docs/reverse-engineering/plat_132/recovered/plat_probe.c`.

## Open Questions

- The exact type and source of `CspGetPortInfo` are external to this module.
- Exact meanings of the sysctrl `+0x10` field and efuse `+0x44` field remain
  unknown.
- The precise semantics of the two ioremap flag values require ARM64 vendor
  memory-management evidence.
- The complete OF match-table contents are still to be reconstructed.
