# plat_132.ko Reconstruction Plan

## Objective

Recover a maintainable, evidence-backed, source-like representation of
`plat_132.ko`, one function at a time. The immediate target is semantic C
reconstruction and a complete interface map. A buildable replacement is a
later goal because vendor register definitions, kernel configuration, and
board-specific headers are incomplete.

## Scope

- Target: `vendor-reference/sr1010-vendor-runtime/modules/files/kmodule/plat_132.ko`
- Target hash: `6cb3e7c9567b4549b5d1b1c6d87f502d3b07fcde0461c2d28a5ad115661d4d51`
- Architecture: ARM64 relocatable kernel module, Linux 5.4.196.
- Primary analysis tool: IDA session `plat132-analysis-reopen`.
- Evidence sources: the target module, its IDA database, collected runtime
  state under `vendor-reference/sr1010-vendor-runtime/`, and companion module
  interfaces only when needed to resolve a cross-module boundary.
- Excluded source tree: `linux-6.18.38/` is mainline code and must not be used
  as evidence for vendor implementation, function semantics, register layout,
  or control flow. It may only be consulted later for generic API vocabulary if
  explicitly labeled as non-vendor context.
- Secondary ABI reference only: `/Volumes/code/linux-5.4.196` may be used to
  identify upstream Linux 5.4.196 API names, likely callback slots, and a
  candidate baseline structure layout. It is not vendor source: the vendor
  kernel is expected to be modified, so it cannot establish behavior, register
  definitions, offsets, ownership rules, or missing code paths.

## Non-Goals

- Do not patch the vendor module or alter runtime evidence.
- Do not claim original source-level names, types, or bit definitions unless
  they are directly evidenced.
- Do not call a reconstructed function buildable until its required vendor
  types, register definitions, and kernel interfaces have been validated.
- Do not use the mainline `linux-6.18.38/` tree to fill gaps in vendor code.
- Do not use `/Volumes/code/linux-5.4.196` to fill gaps in vendor code. Any
  reference to it must be labeled `upstream ABI reference` and checked against
  the module binary and vendor runtime evidence.

## Continuity Protocol

1. At the start of every future analysis turn, read `MEMORY.md` and this file.
2. Work on one function at a time. Do not mark it complete merely because
   Hex-Rays produced pseudocode.
3. Before ending a turn, update both the function ledger in this file and
   `MEMORY.md` with durable discoveries, unresolved questions, and the next
   exact function to process.
4. Record every semantic IDA change (rename, type, comment, or data layout)
   with address and rationale. Avoid changing the shared vendor IDB unless the
   change has been validated and recorded.
5. Preserve certainty labels: `verified`, `strong inference`, or `unknown`.
6. Treat `vendor-reference` files as immutable evidence. Reconstructed source
   and notes belong under this directory.

## Per-Function Completion Criteria

For each function, record all applicable items:

- Entry address, current IDA name, size, and caller/callee context.
- C-like signature and all inferred argument/return semantics.
- Referenced globals, MMIO ranges, descriptor fields, locks, callbacks, and
  allocation/free ownership.
- Control-flow summary including error paths and concurrency context.
- Evidence used: decompiler, assembly, xrefs, runtime logs, or companion API.
- When an upstream kernel reference is used, identify it separately from
  implementation evidence and record any suspected vendor divergence.
- Confidence label and unresolved fields or constants.
- Source-like output location when reconstruction files are introduced.

## Reconstruction Order

### Phase 0: Inventory and Type Baseline

- [ ] Export an exhaustive function ledger for all 656 functions.
- [ ] Inventory globals, exported symbols, imports, structures, and callback
      tables before applying semantic types.
- [ ] Establish recovered header layout and naming conventions.

### Phase 1: Module and Platform Lifecycle

- [x] `0x1c3b4 init_module`
- [x] `0x0e8c pon_driver_register`
- [x] `0x0580 zx_pon_probe`
- [x] `0x0308 zx_pon_remove`
- [x] `0x11a50 nppt_init`
- [x] `0x16a0 register_pon_int`
- [x] `0x1710 register_nppt_int`
- [x] `0x12bc zx_pon_int`
- [x] `0x0ed4 zx_nppt_int`

### Phase 2: IDM Bring-Up, Memory, and IRQ Backend

- [x] `0x14ff4 idm_init`
- [x] `0x14d88 idm_cfg_int`
- [x] `0x13a78 idm_int_enable`
- [x] `0x13ad8 idm_int_disable`
- [x] `0x13bb0 idm_cpu_int`
- [x] `0x13b88 idm_wifi_int`
- [x] `0x13b60 idm_rls_int`
- [x] `0x13b38 idm_all_int`
- [x] `0x13088 idm_get_cpu_rx_qc`
- [x] `0x1309c idm_get_cpu_tx_q`
- [x] `0x137fc idm_get_cpu_rx_cnt`
- [x] `0x13864 idm_get_tx_done`
- [x] `0x138f8 idm_get_reorder_rls`
- [x] `0x14144 idm_rx_refill0`

### Phase 3: Linux Netdev Glue and RX

- [x] `0x0e1ec cpu_register_netinfo`
- [x] `0x0e220 cpu_net_init`
- [x] `0x0e524 dump_net_int_info`
- [x] `0x0e5c8 sub_E5C8`
- [x] `0x0e5e0 __fswab32`
- [x] `0x0e5e8 virt_to_phys`
- [x] `0x0fa60 sub_FA60`
- [x] `0x0fa68 testftp_net_report`
- [x] `0x0fb6c testftp_init`
- [x] `0x10414 buf_fifo_rls`
- [x] `0x104c8 buf_fifo_rls_all`
- [x] `0x104f8 idm_recycle_stats`
- [x] `0x1063c idm_recycle_init`
- [x] `0x0b010 get_next_rxdesc`
- [x] `0x0b1d0 cpu_net_register`
- [x] `0x0b8d0 cpu_net_open`
- [x] `0x0b9b0 cpu_net_stop`
- [x] `0x0b2a8 cpu_net_timeout`
- [x] `0x0e188 cpu_net_int`
- [x] `0x0cce4 cpu_net_poll`
- [x] `0x0cb20 cpu_idm_poll`
- [x] `0x0c294 idm_net_poll`
- [x] `0x0b86c cpu_rls_poll`
- [x] `0x0c5dc cpu_net_rx`
- [x] `0x0bf6c idm_net_rx`
- [x] `0x0c3ec cpu_sw_rx`
- [x] `0x0c4b0 cpu_omci_rx`

### Phase 4: TX, Reclamation, and Offloads

- [x] `0x0d668 cpu_net_tx`
- [x] `0x0dd78 idm_tx_test`
- [x] `0x0e004 net_tst_tx`
- [x] `0x0e0c0 oam_tx`
- [x] `0x0e0d8 net_omci_tx_test`
- [x] `0x0d234 idm_net_tx`
- [x] `0x0d5ac cpu_net_pon_set_desc`
- [x] `0x14a30 idm_cpu_tx`
- [x] `0x1493c idm_omci_tx`
- [x] `0x14be4 idm_wifi_tx`
- [x] `0x0b050 net_check_reorder_rls_nolock`
- [x] `0x0b1a0 dev_kfree_skb_any`
- [x] `0x0b1b8 napi_complete`
- [x] `0x0b2ec dump_net_condition_set`
- [x] `0x0b700 cpu_timer_unlock`
- [x] `0x0b768 do_raw_spin_lock`
- [x] `0x0bae4 dump_net_check`
- [x] `0x0bc30 dump_net_data`
- [x] `0x0bcb8 dump_net_desc`
- [x] `0x0bd8c idm_set_wifi_trap_info`
- [x] `0x0bf3c cpu_dev_stat`
- [x] `0x0bf58 cpu_eth_get_stats`
- [x] `0x0c3cc cpu_net_free_buf`
- [x] `0x0ba8c __raw_spin_lock_irqsave`
- [x] `0x0af5c arch_local_irq_restore`
- [x] `0x0fb7c arch_local_irq_save`
- [x] `0x0fb94 arch_local_irq_restore_0`
- [x] `0x0fba0 __my_cpu_offset`
- [x] `0x0fba8 do_raw_spin_lock_flags.isra.2`
- [x] `0x0fc28 do_raw_spin_lock_0`
- [x] `0x0fbe4 _buf_fifo_free_data`
- [x] `0x0fc64 buf_fifo_free_data`
- [x] `0x1003c buf_fifo_alloc_data`
- [x] `0x1029c idm_skb_stack_pop`
- [x] `0x10354 net_alloc_skb`
- [x] `0x10380 net_alloc_kmem`
- [x] `0x103ac net_free_kmem`
- [x] `0x103dc idm_skb_stack_wifi_push`
- [x] `0x0fec4 _idm_skb_stack_push`
- [x] `0x0ffd8 idm_skb_stack_push`
- [x] `0x0b4fc net_check_tx_done_nolock`
- [x] `0x0b7a4 cpu_timer_func`
- [x] `0x0f87c net_gso_tx`
- [x] `0x10eac pp_net_tcp_gro`
- [x] `0x10a9c can_tcp_gro`
- [x] `0x10930 is_l4port_supported`
- [x] `0x10e5c search_gro_flow`
- [x] `0x10c24 pp_tcp_gro_flush`
- [x] `0x10dcc pp_tcp_gro_flush_all`
- [x] `0x1150c net_gro_init`
- [x] `0x10778 lower_net_smb_test_config`
- [x] `0x10750 __fswab32_0`
- [x] `0x10758 hlist_del_init`
- [x] `0x10738 sub_10738`
- [x] `0x10808 __raw_spin_lock_bh.constprop.13`
- [x] `0x10864 add_supported_l4port`
- [x] `0x109d0 remove_supported_l4port`
- [x] `0x0f9bc net_gso_init`
- [x] `0x0ea40 net_upload_fun`
- [x] `0x0eb50 upload_write_proc`
- [x] `0x0e964 gso_upload_enable`
- [x] `0x0e89c gso_upload_disable`
- [x] `0x0afd8 cpu_net_alloc_nbuf`
- [x] `0x0b4b0 cpu_net_free_nbuf`
- [x] `0x0e634 net_gso_upload_send`
- [x] `0x0ec3c net_tcp_gso_tx_upload`
- [x] `0x0ef38 net_tcp_gso_tx_upload1`
- [x] `0x0f258 net_tcp_gso_tx`
- [x] `0x0e7f4 net_gso_checksum_upload`
- [x] `0x0e788 net_gso_ipv6tcp_checksum.constprop.6`
- [x] `0x0df4c net_cfg_desc_by_skb`
- [x] `0x0dfa8 cpu_net_nb_desc_tx`
- [x] `0x0ce8c net_get_next_txdesc`
- [x] `0x0aff8 net_set_prev_txdesc`
- [x] `0x0cf14 cpu_net_nb_tx`
- [x] `0x0d49c cpu_lowpower_tx`
- [x] `0x0afc0 regisetr_low_power_send_pkt_handle`
- [x] `0x0afcc regisetr_low_power_up_en_judge_handle`
- [x] `0x0af68 register_omci_oam_handle`
- [x] `0x0af74 regisetr_omci_mic_add_handle`
- [x] `0x0af80 idm_omci_portid_set`
- [x] `0x0af84 register_omci_mic_check_handle`
- [x] `0x0af90 register_woe_recycle_handle`
- [x] `0x0af9c register_woe1_recycle_handle`
- [x] `0x0afa8 register_woe2_recycle_handle`
- [x] `0x0afb4 register_wlan_to_essid_handle`

### Phase 5: MAC, PHY, SerDes, and PON Control

- [x] `0x129c8 nppt_smac_init`
- [x] `0x128ec smac_thread_init`
- [x] `0x12890 smac_check_phy_task_thread`
- [x] `0x126e4 check_phy`
- [x] `0x1295c nppt_smac_set_uni_mode`
- [x] `0x11fe0 sopc_send_enable`
- [x] `0x11fcc sub_11FCC`
- [x] `0x12178 nppt_smac_disable`
- [x] `0x12250 nppt_smac_enable`
- [x] `0x12358 smac_reset`
- [x] `0x123ec nppt_smac_config_speed_duplex`
- [x] `0x182c4 xmac_rlt_phy_init`
- [x] `0x182e4 xmac_mvl_phy_init`
- [x] `0x18324 xmac_bcm_phy_init`
- [x] `0x18328 xmac_aqr_phy_init`
- [x] `0x18348 xmac_zxic_phy_init`
- [x] `0x1845c xmac_jl_phy_init`
- [x] `0x18460 xmac_init`
- [x] `0x17da0 xmac_init_by_work_mode`
- [x] `0x1718c xmac_10gbase_r_conf`
- [x] `0x17280 xmac_5gbase_r_conf`
- [x] `0x1781c xmac_1gbase_x_conf`
- [x] `0x16d6c xamc_init_conf_by_speed`
- [x] `0x16984 xmac_get_nppt_glb_link_status`
- [x] `0x169e4 xmac_reset`
- [x] `0x16a0c xmac_tx_disable`
- [x] `0x16a70 xmac_tx_enable`
- [x] `0x16ad4 xmac_rx_disable`
- [x] `0x16b48 xmac_rx_enable`
- [x] `0x16bbc xmac_tx_rx_enable`
- [x] `0x16bdc xmac_tx_rx_disable`
- [x] `0x16ee4 xmac_sgmii_conf`
- [x] `0x17378 xmac_2pt5gbase_x_conf`
- [x] `0x17484 xmac_10g_usxgmii_auto_conf`
- [x] `0x175b0 xmac_5g_usxgmii_auto_conf`
- [x] `0x176dc xmac_2pt5g_usxgmii_auto_conf`
- [x] `0x17938 xmac_hsgmii_conf`
- [x] `0x16bfc xmac_set_duplex_mode`
- [x] `0x16c84 xmac_get_duplex_mode`
- [x] `0x16cdc xmac_get_uni_speed_from_xmac`
- [x] `0x1670c xmac_set_speed_sel`
- [x] `0x17bd8 xmac_mode_set`
- [x] `0x17d38 xmac_set_sopc_duplex_mode`
- [x] `0x17f24 xmac_sopc_send_enable`
- [x] `0x17fdc xmac_switch_uni_speed_to_xmac_speed`
- [x] `0x18058 xmac_speed_process_in_sgmii_auto_mode`
- [x] `0x18130 xmac_config_speed_duplex`
- [x] `0x18530 xmac_speed_process_in_usxgmii_auto_mode`
- [x] `0x1860c xmac_speed_process`
- [x] `0x1874c xmac_set_pcs_for_sgmii_half_duplex`
- [x] `0x187f0 xpcs_set_sr_pma_ctrl1_low_power_en`
- [x] `0x18870 xpcs_set_sr_xs_pcs_ctrl1_low_power_en`
- [x] `0x188f0 xpcs_set_sr_xs_pcs_ctrl2_pcs_type`
- [x] `0x18930 xpcs_set_vr_xs_pcs_dig_ctrl1_usxg_en`
- [x] `0x189b0 xpcs_set_vr_xs_pcs_dig_ctrl1_vr_rst`
- [x] `0x18a30 xpcs_eee_cfg`
- [x] `0x18b3c xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared`
- [x] `0x18bcc xpcs_wait_vr_xs_pcs_dig_sts_pseq_state.constprop.0`
- [x] `0x18c68 xpcs_set_vr_xs_pcs_xaui_ctrl_xaui_mode.constprop.1`
- [x] `0x18cdc xpcs_set_sr_xs_pcs_ctrl1_speed_sel.constprop.2`
- [x] `0x18d50 xpcs_set_sr_pma_ctrl_speed_sel.constprop.3`
- [x] `0x18f44 xpcs_set_sr_mii_ctrl_speed`
- [x] `0x18dc4 xpcs_set_vr_xs_pcs_dig_ctrl1_2_5g_mode_en`
- [x] `0x18ec4 xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en`
- [x] `0x18e44 xpcs_set_vr_xs_pcs_dig_ctrl1_usra_rst_en`
- [x] `0x19090 xpcs_speed_duplex_conf_in_auto_disable_usxgmii_mode`
- [x] `0x19010 xpcs_set_sr_mii_ctrl_duplex_mode`
- [x] `0x19104 xpcs_set_sr_mii_ctrl_an_enable`
- [x] `0x19184 xpcs_sr_mii_ctrl_is_an_enable`
- [x] `0x191c8 xpcs_set_vr_mii_dig_ctrl1_2_5g_mode_en`
- [x] `0x19248 xpcs_set_sr_mii_dig_ctrl1_cl37_tmr_ovr_ride`
- [x] `0x192c8 xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw`
- [x] `0x19348 xpcs_set_vr_mii_dig_ctrl1_vsmmd1_en`
- [x] `0x193c8 xpcs_set_vr_mii_an_ctrl_an_intr_en`
- [x] `0x19448 xpcs_auto_negotiation_conf_in_usxgmii_mode`
- [x] `0x194b0 xpcs_set_vr_mii_an_ctrl_pcs_mode`
- [x] `0x1952c xpcs_1g_mode_conf`
- [x] `0x195cc xpcs_set_vr_mii_an_ctrl_tx_config`
- [x] `0x1964c xpcs_set_vr_mii_an_ctrl_sgmii_link_sts`
- [x] `0x196cc xpcs_auto_negotiation_conf_in_sgmii_mode`
- [x] `0x19778 xpcs_set_vr_mii_an_ctrl_mii_ctrl`
- [x] `0x197f8 xpcs_get_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta`
- [x] `0x19840 xpcs_clear_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta`
- [x] `0x198b4 xpcs_switch_vr_mii_an_intr_sts_speed`
- [x] `0x19910 xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode`
- [x] `0x19a50 xpcs_set_vr_mii_link_timer_ctrl`
- [x] `0x19a90 xpcs_auto_negotiation_conf_in_1000base_x_mode`
- [x] `0x19b60 xpcs_exit_hsgmii_mode`
- [x] `0x19bdc xpcs_get_speed_duplex_in_auto_en_sgmii_mode`
- [x] `0x19cd4 xpcs_exit_sgmii_mode`
- [x] `0x19d30 xpcs_exit_usxgmii_mode`
- [x] `0x19d5c xpcs_prepare_for_switch_mode`
- [x] `0x19dcc xpcs_10gbase_r_conf`
- [x] `0x19e54 xpcs_5gbase_r_conf`
- [x] `0x19fe8 xpcs_hsgmii_mode_conf`
- [x] `0x19ee0 xpcs_2p5gbase_x_conf`
- [x] `0x19f74 xpcs_1000base_x_conf`
- [x] `0x19ba4 xpcs_set_speed_duplex_in_sgmii_anto_disale_mode`
- [x] `0x1a210 xpcs_usxgmii_mode_conf`
- [x] `0x1a19c xpcs_sgmii_mode_conf`
- [x] `0x1a370 xpcs_wait_speed_duplex_conf_in_auto_en_usxgmii_mode`
- [x] `0x1a420 xpcs_init`
- [x] `0x1a550 byPassEnableSet`
- [x] `0x1a608 phy_zx5201_check`
- [x] `0x1a688 phy_zx5201_init`
- [x] `0x1a818 phy_8574_check`
- [x] `0x1a8f0 phy_8574_init`
- [x] `0x1abb0 zte_gephy_set_eee_en`
- [x] `0x1ac10 zte_gephy_set_energy_detect_power_down_en`
- [x] `0x1ac58 zte_gephy_set_link_status_change_en`
- [x] `0x1aca4 zte_gephy_get_eee_en_status`
- [x] `0x1ad04 zte_gephy_get_short_reach_en`
- [x] `0x1ad58 zte_gephy_get_energy_detect_power_down_en`
- [x] `0x1ada0 zte_gephy_get_1000m_tx_dac_lv`
- [x] `0x1ae00 zte_gephy_get_1000m_tx_dac_slew`
- [x] `0x1ae60 zte_gephy_get_100m_tx_dac_lv`
- [x] `0x1aec0 zte_gephy_get_100m_tx_dac_slew`
- [x] `0x1af20 zte_gephy_get_link_status_change_en`
- [x] `0x1af6c zte_gephy_get_link_status_change_event`
- [x] `0x1afb8 zte_gephy_get_rx_stats`
- [x] `0x1b048 zte_gephy_set_short_reach_en`
- [x] `0x1b08c zte_gephy_set_ref_clk_25M`
- [x] `0x1b0bc zte_gephy_set_100m_tx_dac_lv`
- [x] `0x1b130 zte_gephy_set_1000m_tx_dac_slew`
- [x] `0x1b1a4 zte_gephy_set_100m_tx_dac_slew`
- [x] `0x1b218 zte_gephy_set_1000m_tx_dac_lv`
- [x] `0x1b28c check_phy_gephy`
- [x] `0x1b340 phy_zxicge_init`
- [x] `0x1b3a0 zte_set_gephy_enable`
- [x] `0x1b3f4 zte_get_gephy_enable`
- [x] `0x1b430 phy_zxic051_get_linkstate`
- [x] `0x1b474 phy_zxic051_set_enable`
- [x] `0x1b4a4 phy_zxic051_get_enable`
- [x] `0x1b4b8 phy_zxic051_set_linkmode`
- [x] `0x1b794 phy_zxic051_set_loopback`
- [x] `0x1b9e0 phy_zxic051_get_loopback`
- [x] `0x1ba78 phy_zxic051_get_linkmode`
- [x] `0x1ba8c phy_zxic051_init_check`
- [x] `0x1bb1c phy_zxic051_para_init`
- [x] `0x1bb48 phy_zxic051_port_exist`
- [x] `0x1bb78 phy_zxic_051_phy_uni_check`
- [x] `0x1c3e8 plat_cleanupModule`
- [x] `0x1c400 dg_timer_init`
- [x] `0x1c458 __fswab64`
- [x] `0x1c460 _idm_rx_refill`
- [x] `0x00000 getEponDeactiveState`
- [x] `0x0000c setEponDeactiveState`
- [x] `0x00020 pon_set_8k_out_en`
- [x] `0x00060 pon_set_1pps_out_en`
- [x] `0x000a0 pon_set_uart1_txd_en`
- [x] `0x000dc pon_set_1pps_tod_out_en`
- [x] `0x00104 pon_set_pin_mux_13`
- [x] `0x00128 pon_set_pll_pon_ref_clock`
- [x] `0x0014c pon_set_pll_pon_cfg_with_ref_clk_25M`
- [x] `0x00188 pon_set_pll_pon_en`
- [x] `0x001ac pon_use_pll_pon_ref_from_ex_pll`
- [x] `0x001dc isCpuType_133`
- [x] `0x001f0 isCpuType_132`
- [x] `0x00204 isCpuType_129`
- [x] `0x00218 ponserdes_to_xmac1_en_set`
- [x] `0x00274 pon_sys_soft_reset`
- [x] `0x00324 arm64_kernel_use_ng_mappings`
- [x] `0x0035c pon_int_enable`
- [x] `0x00374 nppt_int_enable`
- [x] `0x0038c pon_soc_pon_core_clk_init`
- [x] `0x003e0 pon_soc_pon_cci_clk_init`
- [x] `0x003fc pon_soc_pon_woe0_clk_init`
- [x] `0x00418 pon_soc_pon_woe1_clk_init`
- [x] `0x00434 pon_soc_pon_tm_clk_init`
- [x] `0x00450 nppt_idm_cci_enable`
- [x] `0x00480 pon_soc_pon_cci_aclk_init`
- [x] `0x0049c pon_soc_pon_tm_aclk_init`
- [x] `0x004d4 pon_soc_pon_nppt_clk_init`
- [x] `0x00548 pon_soc_pon_woe_clk_init`
- [x] `0x00dcc pon_soc_pon_rgmii_clk_set`
- [x] `0x00df4 pps_reset`
- [x] `0x00eb4 pon_driver_unregister`
- [x] `0x00fa8 register_gmac_int`
- [x] `0x00fcc register_xgmac_int`
- [x] `0x00ff4 register_emac_int`
- [x] `0x0101c register_xeumac_int`
- [x] `0x01044 register_xedmac_int`
- [x] `0x0106c register_dg_int`
- [x] `0x01094 register_lp_int`
- [x] `0x010bc register_low_power_int`
- [x] `0x010e4 register_ptp_int`
- [x] `0x0110c register_ptp_stamp_int`
- [x] `0x01134 register_oam_int`
- [x] `0x01154 dg_timer_func`
- [x] `0x011fc epon_set_dg_cnt`
- [x] `0x01258 zxic_gpio_set_value`
- [x] `0x0125c epon_get_llid_state`
- [x] `0x01274 xepon_get_llid_state`
- [x] `0x0128c xgpon_get_onu_state`
- [x] `0x012a4 gpon_get_onu_state`
- [x] `0x015f4 pon_is_registered`
- [x] `0x01780 unregister_pon_int`
- [x] `0x017a8 unregister_nppt_int`
- [x] `0x017d0 apb_write`
- [x] `0x017d8 apb_read`
- [x] `0x017e0 apb_bit_write`
- [x] `0x01808 an1_pll_en_cfg`
- [x] `0x01824 serdes_err_cnt_reset`
- [x] `0x0184c serdes_unlock`
- [x] `0x01870 an1_pll_en_get`
- [x] `0x018ac an1_pll_bypass_cfg`
- [x] `0x01a38 an1_pll_bypass_get`
- [x] `0x01a74 an1_pll_out_mode_cfg`
- [x] `0x01aa8 an1_pll_out_mode_get`
- [x] `0x01af0 an1_pll_cfg_ring_circle_bisa_set`
- [x] `0x01b28 an1_pll_cfg_ring_circle_bisa_get`
- [x] `0x01b64 an1_pll_cfg_ring_circle_resl_set`
- [x] `0x01b9c an1_pll_cfg_ring_circle_resl_get`
- [x] `0x01bd8 com_pll_cfg_ring_circle_bisa_set`
- [x] `0x01c28 com_pll_cfg_ring_circle_bisa_get`
- [x] `0x01c70 com_pll_cfg_ring_circle_resl_set`
- [x] `0x01ca8 com_pll_cfg_ring_circle_resl_get`
- [x] `0x01ce4 serdes_set_tx_swin`
- [x] `0x01d1c serdes_set_low_power`
- [x] `0x01e28 serdes_set_band`
- [x] `0x01e6c serdes_get_band`
- [x] `0x01ea8 serdes_set_gen_en`
- [x] `0x01ee0 serdes_set_check_en`
- [x] `0x01f18 serdes_set_err_cnt_en`
- [x] `0x01f50 serdes_get_err_cnt`
- [x] `0x01f98 serdes_prbs_err_ok`
- [x] `0x01fc8 serdes_set_error_time`
- [x] `0x02048 serdesPrbsCounterGetHandler`
- [x] `0x0208c serdes_set_loopback_mode`
- [x] `0x02874 serdes_set_rx_eq_mbf`
- [x] `0x028ac serdes_get_rx_eq`
- [x] `0x02970 serdes_set_np_jittery`
- [x] `0x029a8 serdes_get_np_jittery`
- [x] `0x029e4 check_serdes_version`
- [x] `0x02a58 check_serdes_config`
- [x] `0x02bf0 serdes_set_tx_prbs_mode`
- [x] `0x02de8 serdes_set_rx_prbs_mode`
- [x] `0x02f90 serdes_set_sprbsrxbist`
- [x] `0x02fe4 serdes_set_pattern`
- [x] `0x03098 check_serdes_lock`
- [x] `0x03160 get_all_efuse`
- [x] `0x0410c serdes_set_tx_eq`
- [x] `0x0417c serdes_set_pll_open_loop`
- [x] `0x04200 serdes_set_clk_change`
- [x] `0x0424c serdes_set_rx_eq1`
- [x] `0x042b0 serdes_set_rx_eq2`
- [x] `0x04314 serdes_set_rx_eq3`
- [x] `0x04378 serdes_set_lane_mode`
- [x] `0x043b8 serdes_set_error_time_en`
- [x] `0x043ec serdes_get_hard_prbs_cnt`
- [x] `0x04490 serdes_get_prbs_counters`
- [x] `0x04560 mode_epon_cfg`
- [x] `0x04908 mode_10g_epon_nsyn_dpll_cfg`
- [x] `0x04cc4 mode_10g_epon_nsyn_fifo_cfg`
- [x] `0x05080 mode_10g_epon_nsyn_nofifo_cfg`
- [x] `0x0543c mode_10g_epon_syn_cfg`
- [x] `0x05644 mode_gpon_cfg`
- [x] `0x05ba8 mode_gpon_syn_cfg`
- [x] `0x05d90 mode_xgpon_nsyn_cfg`
- [x] `0x06174 mode_xgpon_syn_cfg`
- [x] `0x063ec eth_an1_clk_set`
- [x] `0x064c0 an1_pll_epon_cfg`
- [x] `0x06594 an1_pll_gpon_cfg`
- [x] `0x06664 mode_eth_10gbase_r_cfg`
- [x] `0x06a28 mode_eth_5gbase_r_cfg`
- [x] `0x06df0 mode_eth_2p5gbase_r_cfg`
- [x] `0x071c8 mode_eth_2p5gbase_x_cfg`
- [x] `0x07728 mode_eth_1gbase_x_cfg`
- [x] `0x07c74 an1_pll_clk_set`
- [x] `0x07cc0 serdes_mode_set`
- [x] `0x07d58 pon_serdes_init`
- [x] `0x07f20 pon_pll_cfg`
- [x] `0x08088 zx_pon_clk_reset_init`
- [x] `0x0829c uni_apb_write`
- [x] `0x082a4 uni_apb_read`
- [x] `0x082ac uni_apb_bit_write`
- [x] `0x082d4 uni_serdes_err_cnt_reset`
- [x] `0x082fc uni_serdes_set_pattern`
- [x] `0x0834c zx_uni_clk_reset_init`
- [x] `0x08354 uni_com_pll_cfg_ring_circle_bisa_set`
- [x] `0x083a4 uni_com_pll_cfg_ring_circle_bisa_get`
- [x] `0x083ec uni_com_pll_cfg_ring_circle_resl_set`
- [x] `0x08424 uni_com_pll_cfg_ring_circle_resl_get`
- [x] `0x08460 uni_serdes_set_tx_swin`
- [x] `0x08498 uni_serdes_set_low_power`
- [x] `0x085a4 uni_serdes_set_band`
- [x] `0x085e8 uni_serdes_get_band`
- [x] `0x08624 uni_serdes_set_gen_en`
- [x] `0x0865c uni_serdes_set_check_en`
- [x] `0x08694 uni_serdes_set_err_cnt_en`
- [x] `0x086cc uni_serdes_get_err_cnt`
- [x] `0x08714 uni_serdes_prbs_err_ok`
- [x] `0x08744 uni_serdes_set_error_time_en`
- [x] `0x08778 uni_serdes_set_error_time`
- [x] `0x087fc uni_serdesPrbsCounterGetHandler`
- [x] `0x08840 uni_serdes_set_rx_eq_mbf`
- [x] `0x08878 uni_serdes_get_rx_eq`
- [x] `0x0893c uni_serdes_set_np_jittery`
- [x] `0x08974 uni_serdes_get_np_jittery`
- [x] `0x089b0 pin_mux_debug_clk_133_out0`
- [x] `0x08a68 pin_mux_debug_clk_133_out1`
- [x] `0x08b08 uni_check_serdes_config`
- [x] `0x08c00 uni_serdes_set_loopback_mode`
- [x] `0x0941c uni_serdes_set_tx_prbs_mode`
- [x] `0x09560 uni_serdes_set_rx_prbs_mode`
- [x] `0x096a8 uni_serdes_set_sprbsrxbist`
- [x] `0x09784 uni_check_serdes_lock`
- [x] `0x097dc uni_serdes_get_hard_prbs_cnt`
- [x] `0x09880 uni_serdes_get_prbs_counters`
- [x] `0x09940 uni_serdes_reset`
- [x] `0x09ae8 uni_serdes_set_tx_eq`
- [x] `0x09b58 uni_serdes_set_pll_open_loop`
- [x] `0x09bdc uni_serdes_set_clk_change`
- [x] `0x09c28 uni_serdes_set_rx_eq1`
- [x] `0x09c8c uni_serdes_set_rx_eq2`
- [x] `0x09cf0 uni_serdes_set_rx_eq3`
- [x] `0x09d54 uni_mode_eth_10gbase_r_cfg`
- [x] `0x09f54 uni_mode_eth_5gbase_r_cfg`
- [x] `0x0a154 uni_mode_eth_2p5gbase_r_cfg`
- [x] `0x0a354 uni_mode_eth_2p5gbase_x_cfg`
- [x] `0x0a6fc uni_mode_eth_1gbase_x_cfg`
- [x] `0x0aa90 uni_serdes_mode_set`
- [x] `0x0aae8 uni_zx_serdes_init`
- [x] `0x0acb0 uni_pll_cfg`
- [x] `0x0ada8 uni_eth_mode_change`
- [x] `0x0ae34 uni_serdes_init`
- [x] `0x0af44 uni_serdes_on_133`
- [x] `0x1bfa4 phy_051_set_xmac_work_mode`
- [x] `0x1c00c phy_051_set_xmac_speed`
- [x] `0x1c0c0 phy_zxic051_check`
- [x] `0x11520 __raw_readl`
- [x] `0x11528 timer_refresh_config_load_reg`
- [x] `0x1154c timer0_process`
- [x] `0x1158c timer_int_handler`
- [x] `0x11614 timer0_config`
- [x] `0x11664 timer0_start`
- [x] `0x11680 timer0_stop`
- [x] `0x11698 zx_timer0_stop`
- [x] `0x116ac timer1_init`
- [x] `0x116fc timer1_get_counter`
- [x] `0x11728 timer0_register_func`
- [x] `0x11734 zx_timer_wclk_sel`
- [x] `0x11794 zx_timer_init`
- [x] `0x1193c timer0_config_dothz`
- [x] `0x1198c zx_timer0_start`
- [x] `0x11a10 soam_init`
- [x] `0x11aac nppt_nppu_reset`
- [x] `0x11b3c nppt_tm_reset`
- [x] `0x11bcc nppt_exit`
- [x] `0x11be4 arch_local_irq_save_0`
- [x] `0x11bfc arch_local_irq_restore_1`
- [x] `0x11c08 greg_sdet_to_reset`
- [x] `0x11c64 greg_init_done_check`
- [x] `0x11ce8 greg_sdet_to_restore`
- [x] `0x11d54 do_raw_spin_lock_flags.isra.1.constprop.3`
- [x] `0x11d98 greg_sopc_auto_gate_en_get`
- [x] `0x11df8 greg_sopc_auto_gate_en_set`
- [x] `0x11e60 greg_smac0_3_mask_runt_err`
- [x] `0x11e80 greg_smac6_mask_runt_err`
- [x] `0x11e98 greg_xmac_mask_runt_type`
- [x] `0x11ebc greg_smac_mask_runt_err`
- [x] `0x11f00 greg_init`
- [x] `0x11f60 greg_rgmii_intf_mode_set`
- [x] `0x11f84 greg_sdet_share_clk_cfg`
- [x] `0x1293c smac_del_phy_scan`
- [x] `0x12980 nppt_smac_set_rgmii_mode`
- [x] `0x12f88 test_and_set_bit`
- [x] `0x13008 __fswab32_1`
- [x] `0x13010 virt_to_phys_0`
- [x] `0x1305c __my_cpu_offset_0`
- [x] `0x13064 arch_local_irq_save_1`
- [x] `0x1307c arch_local_irq_restore_2`
- [x] `0x130c8 idm_rx_update`
- [x] `0x130f0 idm_rx_test`
- [x] `0x130f8 idm_recv_debug_set`
- [x] `0x130fc idm_tx_debug_set`
- [x] `0x13108 idm_rx_debug_set`
- [x] `0x13114 idm_wifi_tx_debug_set`
- [x] `0x13120 idm_wifi_rx_debug_set`
- [x] `0x13134 idm_omci_tx_debug_set`
- [x] `0x13140 idm_omci_rx_debug_set`
- [x] `0x1314c idm_set_smct_all_trap`
- [x] `0x13174 set_last_extral_cnt`
- [x] `0x13184 set_last_normal_cnt`
- [x] `0x13194 set_last_jumbo_cnt`
- [x] `0x131a4 get_last_buffer_idx`
- [x] `0x131f0 set_last_buffer_idx`
- [x] `0x13234 idm_stat`
- [x] `0x13568 idm_debug_stat`
- [x] `0x136bc idm_print_bppe`
- [x] `0x136dc data_padding`
- [x] `0x13740 idm_rls_update`
- [x] `0x137c4 idm_cpu_nb_tx_update`
- [x] `0x13898 idm_get_smct_all_trap`
- [x] `0x1397c get_order`
- [x] `0x139a4 set_idm_int_cpu_rx_cpu_config`
- [x] `0x13a2c do_raw_spin_lock_flags.isra.1.constprop.21`
- [x] `0x13bd8 do_raw_spin_lock_1`
- [x] `0x13c14 idm_rx_refill_flush`
- [x] `0x13d4c idm_rx_refill_reuse`
- [x] `0x13df8 idm_alloc_buf`
- [x] `0x14034 idm_alloc_nbuf`
- [x] `0x142b0 idm_fifo_in`
- [x] `0x143c4 idm_free_buf`
- [x] `0x14604 idm_free_skb_data`
- [x] `0x148b4 dump_tx_desc`
- [x] `0x14b8c dump_tx_desc_wifi`
- [ ] Reconstruct remaining XMAC mode setters and PHY family dispatcher.
- [ ] Reconstruct PON mode, clock, reset, and SerDes state machines.

### Phase 6: Remaining Functions and Public Interface

- [ ] Process all remaining functions in ascending address order, grouped only
      after their shared globals and call graph are documented.
- [x] Inventory exported callback registration APIs and module boundaries:
      `CALLBACK_INTERFACES.md`.
- [ ] Resolve high-value `switch.ko`, `np.ko`, and PHY-module call sites only
      where `plat_132` behavior cannot be established internally.

### Phase 7: Integration Review

- [ ] Cross-check all recovered flows against runtime dmesg, interrupts,
      netdev state, and kallsyms.
- [ ] Identify every unresolved descriptor bit and MMIO register definition.
- [x] Produce a completeness report with confidence levels and build blockers:
      `COMPLETENESS.md`.

## Current Function Ledger

- Completed: `0x1c3b4 init_module`, `0x0e8c pon_driver_register`,
  `0x0580 zx_pon_probe`, `0x0308 zx_pon_remove`, `0x11a50 nppt_init`,
  `0x16a0 register_pon_int`, `0x1710 register_nppt_int`, and
  `0x12bc zx_pon_int`, `0x0ed4 zx_nppt_int`, `0x14ff4 idm_init`, and
  `0x14d88 idm_cfg_int`, `0x13a78 idm_int_enable`, and
  `0x13ad8 idm_int_disable`, `0x13bb0 idm_cpu_int`, and
  `0x13b88 idm_wifi_int`, `0x13b60 idm_rls_int`, and
  `0x13b38 idm_all_int`, `0x13088 idm_get_cpu_rx_qc`, and
  `0x1309c idm_get_cpu_tx_q`, `0x137fc idm_get_cpu_rx_cnt`, and
  `0x13864 idm_get_tx_done`, `0x14144 idm_rx_refill0`, and
  `0x0e1ec cpu_register_netinfo`, `0x0e220 cpu_net_init`, and
  `0x0b1d0 cpu_net_register`, `0x0b8d0 cpu_net_open`,
  `0x0b9b0 cpu_net_stop`, `0x0b2a8 cpu_net_timeout`, `0x0e188 cpu_net_int`,
  `0x0cce4 cpu_net_poll`, `0x0cb20 cpu_idm_poll`, `0x0c294 idm_net_poll`,
  `0x0b86c cpu_rls_poll`, `0x0c5dc cpu_net_rx`, `0x0bf6c idm_net_rx`,
  `0x0c3ec cpu_sw_rx`, `0x0c4b0 cpu_omci_rx`, `0x0d668 cpu_net_tx`,
  `0x0d234 idm_net_tx`, `0x0d5ac cpu_net_pon_set_desc`, `0x14a30 idm_cpu_tx`,
  `0x1493c idm_omci_tx`, `0x14be4 idm_wifi_tx`,
  `0x0b4fc net_check_tx_done_nolock`, `0x0b7a4 cpu_timer_func`, and
   `0x0f87c net_gso_tx`, `0x10eac pp_net_tcp_gro`, `0x10a9c can_tcp_gro`, and
   `0x10930 is_l4port_supported`, `0x10e5c search_gro_flow`, and
   `0x10c24 pp_tcp_gro_flush`, `0x10dcc pp_tcp_gro_flush_all`, and
   `0x1150c net_gro_init`, `0x10778 lower_net_smb_test_config`, and
   `0x10750 __fswab32_0`, `0x10738 sub_10738`,
   `0x10808 __raw_spin_lock_bh.constprop.13`, `0x10864 add_supported_l4port`,
   `0x109d0 remove_supported_l4port`, `0x0f9bc net_gso_init`,
   `0x0ea40 net_upload_fun`, `0x0eb50 upload_write_proc`,
   `0x0e964 gso_upload_enable`, `0x0e89c gso_upload_disable`,
   `0x0afd8 cpu_net_alloc_nbuf`, `0x0b4b0 cpu_net_free_nbuf`,
   `0x0e634 net_gso_upload_send`, `0x0ec3c net_tcp_gso_tx_upload`,
   `0x0ef38 net_tcp_gso_tx_upload1`, `0x0f258 net_tcp_gso_tx`,
   `0x0e7f4 net_gso_checksum_upload`,
   `0x0e788 net_gso_ipv6tcp_checksum.constprop.6`,
   `0x0df4c net_cfg_desc_by_skb`, `0x0dfa8 cpu_net_nb_desc_tx`, and
   `0x0ce8c net_get_next_txdesc`, `0x0aff8 net_set_prev_txdesc`, and
   `0x0cf14 cpu_net_nb_tx`, `0x0d49c cpu_lowpower_tx`, and
   `0x0afc0 regisetr_low_power_send_pkt_handle`, and
   `0x0afcc regisetr_low_power_up_en_judge_handle`, and
   `0x0af68 register_omci_oam_handle`, and
   `0x0af74 regisetr_omci_mic_add_handle`, and
   `0x0af80 idm_omci_portid_set`, and
    `0x0af84 register_omci_mic_check_handle`, and
    `0x0af90 register_woe_recycle_handle`, and
    `0x0af9c register_woe1_recycle_handle`, and
    `0x0afa8 register_woe2_recycle_handle`, and
    `0x0afb4 register_wlan_to_essid_handle`, and
    `0x0b010 get_next_rxdesc`, and
    `0x0b050 net_check_reorder_rls_nolock`, and
    `0x0b1a0 dev_kfree_skb_any`, and
    `0x0b1b8 napi_complete`, and
    `0x0b2ec dump_net_condition_set`, and
    `0x0b700 cpu_timer_unlock`, and
    `0x0b768 do_raw_spin_lock`, and
    `0x0bae4 dump_net_check`, and
    `0x0bc30 dump_net_data`, and
   `0x0bcb8 dump_net_desc`, `0x0bd8c idm_set_wifi_trap_info`,
   `0x0bf3c cpu_dev_stat`, `0x0bf58 cpu_eth_get_stats`,
   `0x0c3cc cpu_net_free_buf`, `0x0ba8c __raw_spin_lock_irqsave`,
   `0x0af5c arch_local_irq_restore`, `0x0fb7c arch_local_irq_save`,
   `0x0fb94 arch_local_irq_restore_0`, `0x0fba0 __my_cpu_offset`,
   `0x0fba8 do_raw_spin_lock_flags.isra.2`, `0x0fc28 do_raw_spin_lock_0`, and
   `0x0fbe4 _buf_fifo_free_data`, `0x0fc64 buf_fifo_free_data`, and
   `0x1003c buf_fifo_alloc_data`, `0x1029c idm_skb_stack_pop`, and
   `0x10354 net_alloc_skb`, `0x10380 net_alloc_kmem`, and
    `0x103ac net_free_kmem`, `0x103dc idm_skb_stack_wifi_push`,
    `0x0fec4 _idm_skb_stack_push`, `0x0ffd8 idm_skb_stack_push`,
    `0x129c8 nppt_smac_init`, `0x128ec smac_thread_init`,
     `0x12890 smac_check_phy_task_thread`, `0x126e4 check_phy`,
     `0x1295c nppt_smac_set_uni_mode`,
     `0x11fe0 sopc_send_enable`,
     `0x11fcc sub_11FCC`,
     `0x12178 nppt_smac_disable`,
     `0x12250 nppt_smac_enable`,
     `0x12358 smac_reset`,
     `0x123ec nppt_smac_config_speed_duplex`,
     `0x18460 xmac_init`, `0x17da0 xmac_init_by_work_mode`,
    `0x1718c xmac_10gbase_r_conf`, `0x17280 xmac_5gbase_r_conf`,
    `0x1781c xmac_1gbase_x_conf`, `0x16ee4 xmac_sgmii_conf`,
    `0x17378 xmac_2pt5gbase_x_conf`, `0x17484 xmac_10g_usxgmii_auto_conf`,
    `0x175b0 xmac_5g_usxgmii_auto_conf`,
    `0x176dc xmac_2pt5g_usxgmii_auto_conf`, `0x17938 xmac_hsgmii_conf`,
     `0x16bfc xmac_set_duplex_mode`, `0x16c84 xmac_get_duplex_mode`,
     `0x16cdc xmac_get_uni_speed_from_xmac`,
     `0x16d6c xamc_init_conf_by_speed`,
     `0x1670c xmac_set_speed_sel`,
     `0x16984 xmac_get_nppt_glb_link_status`,
     `0x169e4 xmac_reset`,
     `0x16a0c xmac_tx_disable`,
     `0x16a70 xmac_tx_enable`,
     `0x16ad4 xmac_rx_disable`,
     `0x16b48 xmac_rx_enable`,
      `0x16bbc xmac_tx_rx_enable`,
      `0x16bdc xmac_tx_rx_disable`,
      `0x17bd8 xmac_mode_set`,
     `0x17d38 xmac_set_sopc_duplex_mode`,
     `0x17f24 xmac_sopc_send_enable`,
     `0x17fdc xmac_switch_uni_speed_to_xmac_speed`,
      `0x18058 xmac_speed_process_in_sgmii_auto_mode`,
      `0x18130 xmac_config_speed_duplex`,
      `0x18530 xmac_speed_process_in_usxgmii_auto_mode`,
      `0x1860c xmac_speed_process`,
       `0x1874c xmac_set_pcs_for_sgmii_half_duplex`,
       `0x18870 xpcs_set_sr_xs_pcs_ctrl1_low_power_en`,
       `0x188f0 xpcs_set_sr_xs_pcs_ctrl2_pcs_type`,
       `0x18930 xpcs_set_vr_xs_pcs_dig_ctrl1_usxg_en`,
       `0x189b0 xpcs_set_vr_xs_pcs_dig_ctrl1_vr_rst`,
       `0x18a30 xpcs_eee_cfg`,
       `0x18b3c xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared`,
       `0x18c68 xpcs_set_vr_xs_pcs_xaui_ctrl_xaui_mode.constprop.1`,
       `0x18cdc xpcs_set_sr_xs_pcs_ctrl1_speed_sel.constprop.2`,
       `0x18d50 xpcs_set_sr_pma_ctrl_speed_sel.constprop.3`,
       `0x18f44 xpcs_set_sr_mii_ctrl_speed`,
       `0x18dc4 xpcs_set_vr_xs_pcs_dig_ctrl1_2_5g_mode_en`,
       `0x18ec4 xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en`,
       `0x18e44 xpcs_set_vr_xs_pcs_dig_ctrl1_usra_rst_en`,
       `0x19090 xpcs_speed_duplex_conf_in_auto_disable_usxgmii_mode`,
       `0x19010 xpcs_set_sr_mii_ctrl_duplex_mode`,
       `0x19104 xpcs_set_sr_mii_ctrl_an_enable`,
       `0x19184 xpcs_sr_mii_ctrl_is_an_enable`,
       `0x191c8 xpcs_set_vr_mii_dig_ctrl1_2_5g_mode_en`,
       `0x192c8 xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw`,
       `0x19348 xpcs_set_vr_mii_dig_ctrl1_vsmmd1_en`,
       `0x193c8 xpcs_set_vr_mii_an_ctrl_an_intr_en`,
       `0x19448 xpcs_auto_negotiation_conf_in_usxgmii_mode`,
       `0x194b0 xpcs_set_vr_mii_an_ctrl_pcs_mode`,
       `0x1952c xpcs_1g_mode_conf`,
       `0x1964c xpcs_set_vr_mii_an_ctrl_sgmii_link_sts`,
       `0x196cc xpcs_auto_negotiation_conf_in_sgmii_mode`,
       `0x19840 xpcs_clear_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta`,
       `0x198b4 xpcs_switch_vr_mii_an_intr_sts_speed`,
       `0x19910 xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode`,
       `0x19b60 xpcs_exit_hsgmii_mode`,
       `0x19bdc xpcs_get_speed_duplex_in_auto_en_sgmii_mode`,
       `0x19cd4 xpcs_exit_sgmii_mode`,
       `0x19d30 xpcs_exit_usxgmii_mode`,
       `0x19d5c xpcs_prepare_for_switch_mode`,
       `0x19ba4 xpcs_set_speed_duplex_in_sgmii_anto_disale_mode`,
       `0x1a210 xpcs_usxgmii_mode_conf`,
       `0x1a370 xpcs_wait_speed_duplex_conf_in_auto_en_usxgmii_mode`,
       `0x1bfa4 phy_051_set_xmac_work_mode`, `0x1c00c phy_051_set_xmac_speed`, and
       `0x1c0c0 phy_zxic051_check`; evidence records are in
     `functions/` and source-like output is in `recovered/`.
- Completed IDM continuation: `0x12f88 test_and_set_bit`, local ABI helpers
  `0x13008` through `0x1307c`, `0x130c8 idm_rx_update`, and the exported
  RX/debug, SMCT, count, and last-index interfaces through
  `0x131f0 set_last_buffer_idx`, plus diagnostics `0x13234 idm_stat` and
  `0x13568 idm_debug_stat`, output/padding helpers through `0x136dc`, and IDM
  release/queue/SMCT/page-order/IRQ-affinity helpers through `0x13a2c`, plus
  generic refill locking, flush, reuse, allocation, FIFO insertion, buffer
   free, skb-data release, TX descriptor diagnostics, the empty IDM exit hook,
   and the BPPE diagnostic backend plus its wrappers, system-register prefix,
   SIPC initializer, XMAC EEE configuration, XMAC0 WAN port selection, and the
   XMAC status diagnostic, dynamic external-PHY identification, and paired
   SGMII/USXGMII global-status readers, fixed XMAC configuration, and the XMAC
     test-mode dispatcher, XMAC-to-SerDes mode mapping, CPU/XMAC-type gating
     stubs, the empty BCM-PHY-named stub, and ZXIC PHY callback registration
     through `0x1845c xmac_jl_phy_init`;
   records are in `functions/` and source-like output is in
   `recovered/plat_idm.c` and `recovered/plat_smac.c`.
- Initial IDA reconnaissance established the module architecture. All remaining
  functions still require their own evidence record before completion.

Ledger reconciliation found 561 internal code functions. `0x11a0c` is ARM64
`.altinstructions` data and `0x16d30` is a tail-dispatch entry already covered
by its parent; neither is an independent recovery target. Every internal entry
is now covered by a function record or an explicit non-function/tail note.

Next action: cross-check high-impact reconstructed flows against captured
runtime dmesg, interrupt counters, and interface state.
