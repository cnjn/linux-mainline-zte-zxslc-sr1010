# plat_132 Callback Interfaces

## Scope and Evidence

This inventory covers `plat_132` callback registration APIs, exported callback
slots, and observed module-to-module symbol dependencies. A `__ksymtab` record
establishes a kernel export. An undefined symbol in a companion module proves a
link-time dependency only; it does not prove that module writes the slot or
define callback ownership, synchronization, or lifetime.

All addresses below are module-relative text addresses unless noted otherwise.

## Interrupt Registrations

Most interrupt registration APIs have this recovered shape:

```c
unsigned int register_xxx_int(pon_interrupt_callback_t callback,
                              uintptr_t context);
```

They overwrite one callback slot and shared `pon_int_info`, then clear the
specified bit in the PON or NPPT interrupt-mask register. They return the raw
mask word after that update and provide no synchronization or previous-value
return. `register_oam_int` takes only a callback and does not update the shared
context.

| API | Slot | Controller bit |
| --- | --- | --- |
| `0x0fa8 register_gmac_int` | `gpon_isr` | PON `0x001` |
| `0x0fcc register_xgmac_int` | `xgpon_isr` | PON `0x080` |
| `0x0ff4 register_emac_int` | `epon_isr` | PON `0x100` |
| `0x101c register_xeumac_int` | `xeupon_isr` | PON `0x200` |
| `0x1044 register_xedmac_int` | `xedpon_isr` | PON `0x400` |
| `0x106c register_dg_int` | `dg_isr` | PON `0x020` |
| `0x1094 register_lp_int` | `lp_isr` | PON `0x040` |
| `0x10bc register_low_power_int` | `low_power_isr` | PON `0x800` |
| `0x10e4 register_ptp_int` | `ptp_isr` | NPPT `0x400` |
| `0x110c register_ptp_stamp_int` | `ptp_stamp_isr` | NPPT `0x200` |
| `0x1134 register_oam_int` | `oam_isr` | NPPT `0x100` |

`zx_pon_int` dispatches PON slots and `zx_nppt_int` dispatches NPPT slots. The
shared `pon_int_info` is not per-callback state: later registrations replace it.
The low-power dispatcher invokes `low_power_isr` without a null check. `dg_isr`

## Packet and Control Setters

The following exported setters store the supplied pointer exactly and return the
same pointer. They accept null and have no local locking, ownership transfer, or
old-value return.

| API | Published slot | Local consumer |
| --- | --- | --- |
| `0x0af68 register_omci_oam_handle` | `omci_oam_rx` | CPU management RX after optional MIC validation |
| `0x0af74 regisetr_omci_mic_add_handle` | `omci_mic_add` | OMCI/OAM TX; nonzero result drops the skb in applicable modes |
| `0x0af84 register_omci_mic_check_handle` | `omci_mic_check` | CPU management RX; nonzero result rejects applicable frames |
| `0x0af90 register_woe_recycle_handle` | `idm_recycle_cb[0]` | reorder-release callback |
| `0x0af9c register_woe1_recycle_handle` | `idm_recycle_cb[1]` | reorder-release callback |
| `0x0afa8 register_woe2_recycle_handle` | `idm_recycle_cb[2]` | reorder-release callback |
| `0x0afb4 register_wlan_to_essid_handle` | `idm_wlanname_to_essid` | no recovered local consumer |
| `0x0afc0 regisetr_low_power_send_pkt_handle` | `low_power_send` | `cpu_lowpower_tx` |
| `0x0afcc regisetr_low_power_up_en_judge_handle` | `low_power_up_en_judge` | `cpu_lowpower_tx` |

The recovered callback ABIs are:

- `omci_oam_rx`: `void (*)(const void *data, unsigned int length, unsigned int port)`.
- `omci_mic_add` and `omci_mic_check`: `int (*)(const void *data, unsigned int length)`.
- `idm_recycle_cb`: `void (*)(unsigned int queue, zte_recycle_context_t *context)`.
- `low_power_send`: five arguments, observed as `(0, 0, data, length, 0)`.
- `low_power_up_en_judge`: `int (*)(void)`.

`net_check_reorder_rls_nolock` invokes a non-null recycle callback with a
stack-local context. A callback must not retain that context address. The slot
setters themselves can race this dispatch because neither side serializes slot
publication.

## Direct Hook Slots

These exported data objects have no recovered setter within `plat_132`.

| Slot | Recovered ABI / local use |
| --- | --- |
| `switch_skb_recv` | `void (*)(zte_skb_t *)`; normal CPU RX and GRO fallback delivery |
| `idm_skb_recv` | `void (*)(zte_wifi_trap_info_t *, zte_skb_t *)`; IDM and qualifying switch-trap delivery |
| `idm_recv_cmpl` | `void (*)(void)`; called on every `idm_net_poll` exit when non-null |
| `dev_qos_select_queue` | `unsigned int (*)(zte_skb_t *)`; low nine result bits feed PON TX descriptor QoS |
| `dev_qos_select_queue_for_lan` | exported pointer slot; SW-TX consumer is recovered, but no reliable callback ABI is assigned |
| `dev_qos_get_queue` | exported pointer slot with no recovered local use or ABI |

The absence of a local registration API means the writer and lifetime contract
remain outside this module's recovered behavior.

## PHY Callback Tables

Nine exported `sg_xphy_*` objects are 40-byte pointer tables, or five
pointer-sized entries each:

`sg_xphy_enable_set`, `sg_xphy_enable_get`, `sg_xphy_linkstatus_get`,
`sg_xphy_linkmode_set`, `sg_xphy_linkmode_get`, `sg_xphy_loopback_set`,
`sg_xphy_loopback_get`, `sg_xphy_eee_set`, and `sg_xphy_mdi_set`.

`xmac_zxic_phy_init @ 0x18348` conditionally fills indices zero through two of
`phy_zxic051_*` callbacks after ignoring `phy_zxic051_para_init` status. The
`eee_set` and `mdi_set` tables have no recovered writer or entry ABI.

## Observed Companion Dependencies

`nm -u` on captured module artifacts confirms these dependencies:

- `np.ko` imports `register_low_power_int`, both low-power setter APIs,
  `register_wlan_to_essid_handle`, `omci_oam_rx`, `switch_skb_recv`, and all
  nine `sg_xphy_*` tables.
- `switch.ko` imports `switch_skb_recv`, `dev_qos_select_queue`, and
  `dev_qos_select_queue_for_lan`.

No companion implementation behavior is inferred from these imports. In
particular, neither import list establishes who assigns an exported direct hook
slot or when a callback may be removed.

## Primary Evidence

- `recovered/plat_irq.c` and `recovered/plat_smac.c` for interrupt publication
  and dispatch.
- `recovered/plat_cpu_net.c` and `recovered/plat_cpu_tx.c` for packet, MIC,
  recycle, low-power, and QoS consumers.
- `recovered/plat_smac.c` for ZXIC PHY callback-table initialization.
- `vendor-reference/sr1010-vendor-runtime/system/proc/kallsyms` for captured
  `__ksymtab` exports and local/global symbol status.
- `vendor-reference/sr1010-vendor-runtime/modules/files/kmodule/{np,switch}.ko`
  undefined-symbol tables for observed companion dependencies.
