/*
 * Semantic reconstruction of SMAC/XMAC initialization at 0x129c8.
 *
 * Raw NPPT register offsets and XMAC type values are taken from the vendor
 * binary. Their hardware names remain intentionally unspecified.
 */

#include <stdint.h>

#define SMAC_PHY_SLOT_COUNT 7U
#define SMAC_GEPHY_PORT_MAX 3U
#define SMAC_REGISTER_BLOCK(mac) (0x40000U * ((mac) + 1U))
#define SMAC_SOPC_ENABLE_REGISTER 0x343f0U
#define SMAC_RESET_REGISTER 0x2c0004U
#define SMAC_SOPC_READY_REGISTER(mac) \
    (0x34000U + 4U * (((mac) + 0xa5U) & 0x1ffU))
#define SMAC_SOPC_SEND_ENABLE_REGISTER(mac) \
    (0x34000U + 4U * (((mac) + 0xacU) & 0x1ffU))
#define SMAC6_SOPC_READY_REGISTER 0x342acU
#define SMAC6_SOPC_SEND_ENABLE_REGISTER 0x342c8U
#define SMAC_RESET_PREPARE_WORD 0x00ba2203U
#define SMAC_CONFIGURATION_WORD 0x00ba2200U
#define XMAC_SOPC_ENABLE_REGISTER(xmac) \
    (0x34000U + 4U * (((xmac) + 0xb0U) & 0x1ffU))
#define XMAC_SOPC_READY_REGISTER(xmac) \
    (0x34000U + 4U * (((xmac) + 0xa9U) & 0x1ffU))

#define NPPT_U32(offset) (*(volatile uint32_t *)(nppt_base + (offset)))
#define RGMII_U32(offset) (*(volatile uint32_t *)(rgmii_base + (offset)))
#define EFUSE_U32(offset) (*(volatile uint32_t *)(efuse_base + (offset)))

typedef int (*zte_smac_check_phy_t)(uint8_t phy);
typedef int (*zte_smac_init_phy_t)(uint8_t phy);
typedef int (*zte_smac_set_phy_enable_t)(uint8_t phy, uint8_t enable);
typedef int (*zte_smac_get_phy_enable_t)(uintptr_t phy, uint8_t *enable);
typedef void (*phy_zxic051_extended_write_t)(uint8_t phy_id,
                                             uint8_t register_address,
                                             uint32_t selector,
                                             uint32_t value);
typedef void (*phy_zxic051_ge_write_t)(uint8_t phy_id,
                                       uint8_t register_address,
                                       uint32_t value);
typedef uint16_t (*phy_zxic051_ge_read_t)(uint8_t phy_id,
                                          uint8_t register_address);
typedef uint16_t (*phy_zxic051_extended_read_t)(uint8_t phy_id,
                                                uint8_t register_address,
                                                uint32_t selector);
typedef intptr_t (*pon_interrupt_callback_t)(uintptr_t arg0, uintptr_t arg1);
typedef struct {
    uint64_t opaque_0;
    uint64_t opaque_8;
    uint64_t expires;
} recovered_timer_t;

volatile uint32_t *apb_bit_write(volatile uint32_t *address, uint32_t value,
                                 unsigned int width,
                                 unsigned int bit_offset);

static const uint8_t xmac_mode_speed_select[6] = { 7, 4, 3, 2, 5, 0 };
static const uint8_t xmac_speed_select_to_uni_speed[8] = {
    6, 7, 4, 3, 2, 5, 4, 1,
};

static uint8_t phy_8574_has_initialized;
static uint8_t phy_zxicge_has_initialized;
static uint32_t an1_pll_reg_c_snapshot;
static uint32_t an1_pll_reg_10_snapshot;
static uint32_t serdes_reg_c_snapshot;
static uint32_t serdes_reg_10_snapshot;
static uint32_t serdes_reg_54_snapshot;
static uint32_t serdes_reg_64_snapshot;
static uint32_t serdes_reg_74_snapshot;
static uint32_t serdes_loopback_call_count;
static uint32_t serdes_loopback_reg_1c_snapshot;
static uint32_t serdes_loopback_reg_24_snapshot;
static uint32_t serdes_loopback_reg_40_snapshot;
static uint32_t serdes_loopback_reg_48_snapshot;
static uint32_t serdes_loopback_reg_90_snapshot;
static uint32_t serdes_loopback_reg_94_snapshot;

extern uintptr_t sg_zxicgephy_apb_base[];
extern uintptr_t gephy_apb_base;
extern uint32_t outerphy_link[];

extern int printk(const char *format, ...);
extern uint16_t zx_mdio_read_ge_ext(uint8_t phy, uint8_t register_address);
extern void zx_mdio_write_ge_ext(uint8_t phy, uint8_t register_address,
                                 uint32_t value);
extern uint16_t zx_mdio_read(uint8_t phy, uint8_t register_address);
extern void zx_mdio_write(uint8_t phy, uint8_t register_address,
                          uint32_t value);
extern uint64_t read_icc_pmr_el1(void);
extern void write_icc_pmr_el1(uint64_t value);
extern void dsb_sy(void);
extern unsigned int isCpuType_129(void);
extern unsigned int isCpuType_133(void);
extern uintptr_t kallsyms_lookup_name(const char *name);
extern int get_capability_for_product(void);
extern int get_swport_by_logicport(unsigned int logical_port);
extern int check_phy_gephy(uint8_t phy);
extern int phy_zxicge_init(uint8_t unused_phy);
extern int zte_set_gephy_enable(uint8_t phy, uint8_t enable);
extern int zte_get_gephy_enable(uintptr_t phy, uint8_t *enable);
void nppt_smac_set_uni_mode(unsigned int mac, unsigned int mode);
void smac_reset(uint32_t mask);
void sopc_send_enable(uint8_t mac);
extern int xmac_phy_type(void);
extern int xmac_init(unsigned int xmac0_work_mode,
                     unsigned int xmac1_work_mode);
extern int xmac_init_by_work_mode(uint8_t xmac,
                                  unsigned int work_mode);
extern int xpcs_init(uint8_t xmac);
extern int xmac_10gbase_r_conf(uint8_t xmac);
extern int xmac_5gbase_r_conf(uint8_t xmac);
extern int xmac_1gbase_x_conf(uint8_t xmac);
extern int xmac_sgmii_conf(uint8_t xmac, unsigned int value_1,
                           unsigned int value_2, unsigned int value_3);
extern int xmac_2pt5gbase_x_conf(uint8_t xmac);
extern int xmac_10g_usxgmii_auto_conf(uint8_t xmac);
extern int xmac_5g_usxgmii_auto_conf(uint8_t xmac);
extern int xmac_2pt5g_usxgmii_auto_conf(uint8_t xmac);
extern int xmac_hsgmii_conf(uint8_t xmac, unsigned int variant);
void xmac_set_sopc_duplex_mode(uint8_t xmac, unsigned int duplex);
extern void __const_udelay(unsigned long loops);
void xmac_tx_rx_enable(unsigned int xmac);
extern int phy_zxic_051_phy_uni_check(uint8_t phy, unsigned int *phy_state,
                                      uint8_t *phy_link,
                                      uint8_t *outer_speed,
                                      uint8_t *duplex);
extern void phy_zxic_speed_outer2uni(uint8_t *speed);
extern void phy_zxic_051_set_enable(void);
extern int phy_zxic_051_get_enable(void);
extern int phy_zxic_051_get_linkmode(void);
extern void phy_zxic_051_phy_init(uint8_t phy);
extern int phy_zxic051_get_linkstate(uint8_t phy, uint8_t *link,
                                     uint8_t *duplex, uint8_t *speed);
extern int phy_zxic051_set_enable(uint8_t phy, uint8_t enable);
extern int phy_zxic051_get_enable(void);
extern int phy_zxic051_set_linkmode(uint8_t phy, uint8_t force_mode,
                                    uint8_t duplex, uint8_t speed);
extern int phy_zxic051_get_linkmode(void);
extern int phy_zxic051_set_loopback(uint8_t phy, uint8_t enable);
extern int phy_zxic051_get_loopback(uint8_t phy, uint8_t *enabled);
extern int phy_zxic051_para_init(uint8_t phy);
extern unsigned int phy_zxic051_port_exist(uint8_t phy);
extern int phy_zxic051_check(uint8_t phy_id);
extern uint8_t phy_051_g_phy_id_check(uint8_t phy);
extern int __printk_ratelimit(const char *function_name);
extern void phy_zxic_051_uniserdes_mode_check(uint8_t phy,
                                               uint16_t status_register,
                                               uint8_t *outer_speed,
                                               uint8_t *duplex);
extern void high_ber_downspeed_check(uint8_t phy, uint8_t phy_id,
                                     uint8_t phy_link, uint8_t outer_speed);
extern void phy_zxic_051_apb_write(uint8_t phy, uint8_t phy_id,
                                   uint32_t register_address, uint32_t value);
extern void speed_hold_check(uint8_t phy, uint8_t phy_id, uint8_t phy_link,
                             uint8_t outer_speed);
extern phy_zxic051_extended_write_t zx_mdio_write_extended[];
extern phy_zxic051_ge_write_t zx_mdio_write_ge_ext_by_port[];
extern phy_zxic051_ge_read_t zx_mdio_read_ge_ext_by_port[];
extern phy_zxic051_extended_read_t zx_mdio_read_extended[];
extern int is_certain_port_used(uint8_t phy);
extern void nppt_exit(void);
extern uint8_t zx_pon_driver[];
extern void platform_driver_unregister(void *driver);
extern int greg_sdet_share_clk_cfg(uint32_t enable);
extern void greg_rgmii_intf_mode_set(uint8_t mode);
extern recovered_timer_t dg_timer;
extern recovered_timer_t phy_timer;
extern void dg_timer_func(void);
extern unsigned long jiffies;
extern void init_timer_key(recovered_timer_t *timer, void (*function)(void),
                           uint32_t flags, uintptr_t key, uintptr_t name);
extern int del_timer(recovered_timer_t *timer);
extern int add_timer(recovered_timer_t *timer);
extern void *idm_alloc_buf(unsigned int size);
extern uintptr_t virt_to_phys_0(const void *address);
extern uint32_t __fswab32_1(uintptr_t value);
extern uint32_t uBP_BUFFER_OFFSET;
extern uint32_t g_epon_deactive;
extern uint32_t g_pon_cputype;
extern uint8_t product_vid;
extern int arm64_const_caps_ready;
extern int cpu_hwcap_keys[];
extern uint64_t cpu_hwcaps;
extern uint32_t g_pon_work_mode;
extern uint32_t dg_flag;
extern uint32_t pon_registered;
extern uint32_t pon_serdes_mode;
extern uint32_t uni_serdes_mode;
extern uint64_t serdesPrbsCounter;
extern uint64_t iPrbsCounter;
extern uint64_t uni_serdesPrbsCounter;
extern uint64_t uni_iPrbsCounter;
extern uint32_t los_state_prbs;
static uint32_t uni_loopback_count;
static uint32_t uni_loopback_default_1c;
static uint32_t uni_loopback_default_24;
static uint32_t uni_loopback_default_40;
static uint32_t uni_loopback_default_48;
static uint32_t uni_loopback_default_60;
static uint32_t uni_loopback_default_90;
static uint32_t uni_loopback_default_94;
static uint32_t uni_rx_bist_count;
static uint32_t uni_rx_bist_default_from_24;
static uint32_t uni_rx_bist_default_48;
static uint32_t uni_rx_bist_default_94;
extern recovered_timer_t serdes_prbs_counter_timer;
extern recovered_timer_t uni_serdes_prbs_counter_timer;
extern void hw_power_optx_set(unsigned int enable);
extern pon_interrupt_callback_t gpon_isr;
extern pon_interrupt_callback_t xgpon_isr;
extern pon_interrupt_callback_t epon_isr;
extern pon_interrupt_callback_t xeupon_isr;
extern pon_interrupt_callback_t xedpon_isr;
extern uintptr_t dg_isr;
extern pon_interrupt_callback_t lp_isr;
extern pon_interrupt_callback_t low_power_isr;
extern pon_interrupt_callback_t ptp_isr;
extern pon_interrupt_callback_t ptp_stamp_isr;
extern pon_interrupt_callback_t oam_isr;
extern uintptr_t pon_int_info;
extern uint32_t g_pon_irq;
extern uint32_t g_nppt_irq;
extern void free_irq(unsigned int irq, void *context);
extern volatile uint32_t *pin_mux_base;
extern volatile uint32_t *top_crm_base;
extern volatile uint8_t *sys_ctrl_base;
extern uint32_t g_phy_type;
extern volatile uint8_t *pps_base;
extern volatile uint8_t *pon_serdes_pll_base;
extern volatile uint8_t *pon_serdes_base;
extern volatile uint8_t *uni_serdes_base;
extern volatile uint8_t *efuse_base;
void xmac_get_nppt_glb_link_status(unsigned int xmac, int *link_up);
void xmac_get_uni_speed_from_xmac(uint8_t xmac, int *speed);
extern int xpcs_get_speed_duplex_in_auto_en_sgmii_mode(
    uint8_t xmac, unsigned int *uni_speed, unsigned int *duplex,
    unsigned int *auto_status);
extern int xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode(
    uint8_t xmac, unsigned int *uni_speed, unsigned int *duplex,
    unsigned int *auto_status);
void xmac_tx_disable(unsigned int xmac);
void xmac_rx_disable(unsigned int xmac);
void xmac_rx_enable(unsigned int xmac);
void xmac_tx_enable(unsigned int xmac);
void xmac_tx_rx_disable(unsigned int xmac);
extern unsigned int isCpuType_132(void);
extern int xpcs_10gbase_r_conf(uint8_t xmac);
extern int xpcs_5gbase_r_conf(uint8_t xmac);
extern int xpcs_1000base_x_conf(uint8_t xmac, unsigned int speed,
                                 unsigned int duplex);
extern int xpcs_2p5gbase_x_conf(uint8_t xmac);
extern int xpcs_sgmii_mode_conf(uint8_t xmac, unsigned int mode_value,
                                unsigned int config_value);
extern int xpcs_auto_negotiation_conf_in_sgmii_mode(uint8_t xmac,
                                                    uint8_t enable);
extern void xpcs_set_vr_mii_an_ctrl_an_intr_en(uint8_t xmac,
                                               uint8_t enable);
extern void xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw(uint8_t xmac,
                                                   uint8_t enable);
extern void xpcs_set_sr_xs_pcs_ctrl2_pcs_type(uint8_t xmac,
                                               unsigned int pcs_type);
extern void xpcs_set_vr_xs_pcs_xaui_ctrl_xaui_mode_constprop_1(uint8_t xmac);
extern void xpcs_set_sr_pma_ctrl_speed_sel_constprop_3(uint8_t xmac);
extern void xpcs_set_sr_xs_pcs_ctrl1_speed_sel_constprop_2(uint8_t xmac);
extern void xpcs_set_sr_xs_pcs_ctrl1_low_power_en(uint8_t xmac,
                                                   uint8_t enable);
extern void xpcs_set_sr_pma_ctrl1_low_power_en(uint8_t xmac,
                                               uint8_t enable);
extern int xpcs_wait_vr_xs_pcs_dig_sts_pseq_state_constprop_0(uint8_t xmac);
extern void xpcs_set_vr_mii_an_ctrl_pcs_mode(uint8_t xmac,
                                              unsigned int pcs_mode);
extern void xpcs_set_vr_mii_link_timer_ctrl(uint8_t xmac,
                                            unsigned int timer);
extern void xpcs_set_sr_mii_dig_ctrl1_cl37_tmr_ovr_ride(uint8_t xmac,
                                                         uint8_t enable);
extern void xpcs_exit_sgmii_mode(uint8_t xmac);
extern void xpcs_exit_usxgmii_mode(uint8_t xmac);
extern void xpcs_exit_hsgmii_mode(uint8_t xmac);
extern void xpcs_set_vr_xs_pcs_dig_ctrl1_2_5g_mode_en(uint8_t xmac,
                                                       uint8_t enable);
extern void xpcs_set_vr_mii_dig_ctrl1_2_5g_mode_en(uint8_t xmac,
                                                    uint8_t enable);
extern void xpcs_set_vr_xs_pcs_dig_ctrl1_usxg_en(uint8_t xmac,
                                                  uint8_t enable);
extern void xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en(uint8_t xmac,
                                                    uint8_t enable);
extern void xpcs_set_vr_mii_dig_ctrl1_vsmmd1_en(uint8_t xmac,
                                                 uint8_t enable);
extern void xpcs_set_vr_xs_pcs_dig_ctrl1_vr_rst(uint8_t xmac,
                                                 uint8_t enable);
extern void xpcs_set_vr_xs_pcs_dig_ctrl1_usra_rst_en(uint8_t xmac,
                                                      uint8_t enable);
extern int xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared(uint8_t xmac);
extern void xpcs_clear_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta(uint8_t xmac);
extern unsigned int xpcs_switch_vr_mii_an_intr_sts_speed(
    unsigned int reported_speed, unsigned int *uni_speed);
extern int xpcs_usxgmii_mode_conf(uint8_t xmac, unsigned int mode);
extern int xpcs_hsgmii_mode_conf(uint8_t xmac, uint8_t variant);
void xamc_init_conf_by_speed(uint8_t xmac, unsigned int speed);
extern int greg_sopc_auto_gate_en_get(void);
extern void greg_sopc_auto_gate_en_set(unsigned int enable);
void xmac_sopc_send_enable(uint8_t xmac);
extern int uni_serdes_init(uint8_t xmac, unsigned int mode);
extern int byPassEnableSet(uint8_t xmac, uint8_t enable);
extern int xpcs_auto_negotiation_conf_in_usxgmii_mode(uint8_t xmac,
                                                       uint8_t auto_enable);
struct zte_task;
typedef int (*zte_kthread_fn_t)(void *argument);
extern int kthread_should_stop(void);
void nppt_smac_disable(uint8_t mac);
void nppt_smac_enable(uint8_t mac);
void nppt_smac_config_speed_duplex(uint8_t mac, uint8_t speed,
                                   uint8_t duplex);
extern void xmac_config_speed_duplex(uint8_t xmac, unsigned int uni_speed,
                                     unsigned int duplex);
extern void xpcs_set_speed_duplex_in_sgmii_anto_disale_mode(
    uint8_t xmac, unsigned int speed, unsigned int state);
extern unsigned int xpcs_sr_mii_ctrl_is_an_enable(uint8_t xmac);
extern void xpcs_set_sr_mii_ctrl_speed(uint8_t xmac, unsigned int speed);
extern void xpcs_set_sr_mii_ctrl_duplex_mode(uint8_t xmac,
                                              unsigned int state);
extern void xpcs_set_vr_mii_an_ctrl_sgmii_link_sts(uint8_t xmac,
                                                    unsigned int link_status);
extern void xpcs_set_sr_mii_ctrl_an_enable(uint8_t xmac,
                                            unsigned int enable);
void xmac_set_pcs_for_sgmii_half_duplex(unsigned int xmac,
                                         unsigned int configure,
                                         unsigned int speed,
                                         unsigned int state);
extern int msleep_interruptible(unsigned int milliseconds);
void check_phy(uint8_t mac);
int smac_check_phy_task_thread(void *argument);
extern struct zte_task *kthread_create_on_node(zte_kthread_fn_t threadfn,
                                               void *argument, int node,
                                               const char *name);
extern int wake_up_process(struct zte_task *task);
extern void smac_thread_init(void);

extern volatile uint8_t *nppt_base;
extern volatile uint8_t *pon_base;
extern volatile uint8_t *rgmii_base;
extern volatile uint8_t *xmac0_pcs_base;
extern uint32_t g_smac_max_index;
extern uint8_t uni_phy[SMAC_PHY_SLOT_COUNT];
extern int32_t uni_phy_stat[SMAC_PHY_SLOT_COUNT];
extern zte_smac_check_phy_t sg_smac_check_phy[SMAC_PHY_SLOT_COUNT];
extern zte_smac_init_phy_t sg_smac_init_phy[SMAC_PHY_SLOT_COUNT];
extern zte_smac_set_phy_enable_t sg_smac_set_phy_enable[SMAC_PHY_SLOT_COUNT];
extern zte_smac_get_phy_enable_t sg_smac_get_phy_enable[SMAC_PHY_SLOT_COUNT];
extern uint8_t xmac_phy_id[2];
extern int Is_279051_phy;
extern uint32_t g_xmac0_type;
extern uint32_t g_xmac1_type;
extern int32_t sg_xmac_work_mode[];
extern int32_t sg_xpcs_mode[];
extern uint32_t xmac_need_set_work_mode;
extern uint32_t dword_2677C;
extern uint32_t g_ponserdes_to_xmac1;
extern uint8_t g_xmac_work_in_auto[];
extern uintptr_t xphy_check_callbacks[];
extern uintptr_t sg_xphy_enable_set[];
extern uintptr_t sg_xphy_enable_get[];
extern uintptr_t sg_xphy_linkstatus_get[];
extern uintptr_t sg_xphy_linkmode_set[];
extern uintptr_t sg_xphy_linkmode_get[];
extern uintptr_t sg_xphy_loopback_set[];
extern uintptr_t sg_xphy_loopback_get[];
extern uint8_t check_phy_en;
extern uint8_t byte_266BC;
extern uint32_t sg_phy_speed_mode_cur_mac[];
extern uint8_t sg_linkdn_check_times_mac[];
extern uint32_t sg_phy_speed_mode_last_mac[];
extern uint32_t nbaset_flag[];
extern uint8_t sg_reg_val_8017_mac[];
extern uint32_t sg_last_serdes_mode0_54532[];
extern uint32_t sg_last_serdes_mode1_54533[];
extern uint32_t sg_051_interval_cnt_54535[];
extern uint32_t sg_051_re_an_cnt_54536[];
extern uint32_t phy_check_reset_serdes_interval_num;
extern const char *phy_speed[];
extern const char *phy_duplex[];

void smac_reset(uint32_t mask)
{
    uint32_t original_value;
    uint32_t reset_value;
    uint32_t restored_value;

    original_value = NPPT_U32(SMAC_RESET_REGISTER);
    printk("val =0x%x, reg = 0x%px\n", original_value,
           (const void *)(nppt_base + SMAC_RESET_REGISTER));
    reset_value = original_value & ~mask;
    restored_value = original_value | mask;
    NPPT_U32(SMAC_RESET_REGISTER) = reset_value;
    printk("reset val =0x%x\n", reset_value);
    __const_udelay(0x1a36f0UL);
    NPPT_U32(SMAC_RESET_REGISTER) = restored_value;
    printk("smac_reset restore val = 0x%x\n", restored_value);
}

void sopc_send_enable(uint8_t mac)
{
    uint32_t ready;
    unsigned int attempt;

    if (mac <= g_smac_max_index) {
        for (attempt = 10U; attempt != 0U; --attempt) {
            ready = NPPT_U32(SMAC_SOPC_READY_REGISTER(mac));
            printk("read SOPC_CLR_OVER_READY_SMAC = 0x%x, mac = 0x%x\n",
                   ready, mac);
            if ((ready & 1U) != 0U) {
                __const_udelay(0x418958UL);
                NPPT_U32(SMAC_SOPC_SEND_ENABLE_REGISTER(mac)) = 1U;
                printk("write SOPC_SEND_EN_CFG_SMAC,mac = 0x%x\n", mac);
                break;
            }
            __const_udelay(0x418958UL);
        }

        NPPT_U32(SMAC_SOPC_SEND_ENABLE_REGISTER(mac)) = 1U;
        return;
    }

    if (mac != 6U)
        return;

    for (attempt = 0U; attempt < 10U; ++attempt) {
        unsigned int delay_count;

        for (delay_count = 100U; delay_count != 0U; --delay_count)
            __const_udelay(0x418958UL);

        ready = NPPT_U32(SMAC6_SOPC_READY_REGISTER);
        printk("read SOPC_CLR_OVER_READY_SMAC6 = 0x%x, mac = 0x%x,i = %d\n",
               ready, 6U, (int)attempt);
        if ((ready & 1U) != 0U) {
            __const_udelay(0x418958UL);
            NPPT_U32(SMAC6_SOPC_SEND_ENABLE_REGISTER) = 1U;
            printk("write SOPC_SEND_EN_CFG_SMAC6\n");
            return;
        }

        __const_udelay(0x418958UL);
    }
}

void sub_11FCC(void)
{
    uint64_t priority_mask = read_icc_pmr_el1();

    write_icc_pmr_el1((uint32_t)priority_mask ^ 0xe0U);
    write_icc_pmr_el1(priority_mask);
    dsb_sy();
    sopc_send_enable((uint8_t)priority_mask);
}

void nppt_smac_set_uni_mode(unsigned int mac, unsigned int mode)
{
    uintptr_t offset = ((uintptr_t)((mac + 5U) & 0x3fffffffU)) << 2;
    uint32_t value = NPPT_U32(offset);

    NPPT_U32(offset) = (value & 0xfc7fffffU) | mode;
}

int nppt_smac_init(void)
{
    int capability;
    int logical_port;
    uint32_t mac;

    g_smac_max_index = isCpuType_129() == 1 ? 2U : 3U;
    for (mac = 0; mac < SMAC_PHY_SLOT_COUNT; ++mac) {
        uni_phy_stat[mac] = -2;
        sg_smac_check_phy[mac] = 0;
        sg_smac_init_phy[mac] = 0;
        sg_smac_set_phy_enable[mac] = 0;
        sg_smac_get_phy_enable[mac] = 0;
    }

    xmac_phy_id[1] = 1;
    capability = get_capability_for_product();
    for (logical_port = 0; logical_port < capability; ++logical_port) {
        int uni_port = get_swport_by_logicport((unsigned int)logical_port);

        printk("eth%d, uni%d init\n", logical_port, uni_port);
        if ((unsigned int)uni_port <= SMAC_GEPHY_PORT_MAX) {
            sg_smac_check_phy[uni_port] = check_phy_gephy;
            sg_smac_init_phy[uni_port] = phy_zxicge_init;
            sg_smac_set_phy_enable[uni_port] = zte_set_gephy_enable;
            sg_smac_get_phy_enable[uni_port] = zte_get_gephy_enable;
        }
    }

    for (mac = 0; mac <= g_smac_max_index; ++mac) {
        uint32_t smac_offset = SMAC_REGISTER_BLOCK(mac);
        uint32_t reset_mask = 1U << mac;
        uint32_t smac_word;
        zte_smac_init_phy_t init_phy = sg_smac_init_phy[mac];

        if (init_phy != 0)
            init_phy(uni_phy[mac]);

        nppt_smac_set_uni_mode(mac, 0U);
        NPPT_U32(smac_offset) = SMAC_RESET_PREPARE_WORD;
        smac_reset(reset_mask);
        printk("enter smac_init\n");
        NPPT_U32(smac_offset) = SMAC_CONFIGURATION_WORD;
        NPPT_U32(smac_offset + 4U) = 0x3fffU;
        NPPT_U32(smac_offset + 8U) = 0x80000001U;

        /* The result is discarded before the following raw register update. */
        (void)isCpuType_133();
        NPPT_U32(smac_offset + 0xb00U) |= 0x200U;
        NPPT_U32(smac_offset + 0xe0U) = 0x00011200U;
        printk("write smac an ctrl ,mac = 0x%x\n", mac);

        smac_word = NPPT_U32(smac_offset);
        if (mac <= g_smac_max_index) {
            if ((smac_word & 0x2000U) != 0)
                NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) &= ~reset_mask;
            else
                NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) |= reset_mask;
        }
        sopc_send_enable((uint8_t)mac);
        printk("sopc send enable ,mac = 0x%x\n", mac);
        printk("exit smac_init:mac=0x%x\n", mac);
    }

    if (isCpuType_133() == 1 || isCpuType_129() == 1) {
        int phy_type = xmac_phy_type();
        unsigned int xmac0_work_mode;
        unsigned int xmac1_work_mode;
        int skip_usxgmii_auto_negotiation;

        switch (phy_type) {
        case 0:
            g_xmac0_type = Is_279051_phy == 1 ? 4U : 1U;
            xmac0_work_mode = 4U;
            xmac1_work_mode = 0U;
            skip_usxgmii_auto_negotiation = 1;
            break;
        case 1:
            g_xmac1_type = Is_279051_phy == 1 ? 4U : 1U;
            xmac0_work_mode = 0U;
            xmac1_work_mode = 4U;
            skip_usxgmii_auto_negotiation = 1;
            break;
        case 2:
            if (Is_279051_phy == 1) {
                g_xmac0_type = 4U;
                g_xmac1_type = 4U;
            } else {
                g_xmac0_type = 1U;
                g_xmac1_type = 1U;
            }
            xmac0_work_mode = 4U;
            xmac1_work_mode = 4U;
            skip_usxgmii_auto_negotiation = 1;
            break;
        case 3:
            g_xmac0_type = 1U;
            xmac0_work_mode = 3U;
            xmac1_work_mode = 0U;
            skip_usxgmii_auto_negotiation = 1;
            break;
        case 5:
            g_xmac1_type = Is_279051_phy == 1 ? 4U : 1U;
            xmac0_work_mode = 5U;
            xmac1_work_mode = 4U;
            skip_usxgmii_auto_negotiation = 0;
            break;
        case 6:
            if (Is_279051_phy == 1) {
                g_xmac1_type = 4U;
                xmac0_work_mode = 4U;
            } else {
                g_xmac1_type = 1U;
                xmac0_work_mode = 6U;
            }
            xmac1_work_mode = 4U;
            skip_usxgmii_auto_negotiation = 0;
            break;
        case 7:
            g_xmac1_type = 1U;
            xmac0_work_mode = 5U;
            xmac1_work_mode = 5U;
            skip_usxgmii_auto_negotiation = 0;
            break;
        case 8:
            g_xmac1_type = 1U;
            xmac0_work_mode = 2U;
            xmac1_work_mode = 3U;
            skip_usxgmii_auto_negotiation = 0;
            break;
        case 9:
            xmac0_work_mode = 5U;
            xmac1_work_mode = 2U;
            skip_usxgmii_auto_negotiation = 0;
            break;
        case 11:
            xmac0_work_mode = 2U;
            xmac1_work_mode = 2U;
            skip_usxgmii_auto_negotiation = 0;
            break;
        default:
            printk("xmac not support phy type %d\n", phy_type);
            xmac0_work_mode = 0U;
            xmac1_work_mode = 0U;
            skip_usxgmii_auto_negotiation = 1;
            break;
        }

        (void)xmac_init(xmac0_work_mode, xmac1_work_mode);
        if (skip_usxgmii_auto_negotiation == 0 && xmac0_work_mode == 5U)
            (void)xpcs_auto_negotiation_conf_in_usxgmii_mode(0U, 0U);
    }

    smac_thread_init();
    return 0;
}

void nppt_smac_config_speed_duplex(uint8_t mac, uint8_t speed,
                                   uint8_t duplex)
{
    uint32_t initial_max_index;
    uint32_t old_config = 0;
    uint32_t new_config;
    uint32_t smac_mask;
    uint32_t auto_gate_was_enabled = 0;

    if (mac > 6U)
        return;

    initial_max_index = g_smac_max_index;
    if (mac <= initial_max_index)
        old_config = NPPT_U32(SMAC_REGISTER_BLOCK(mac));
    if (mac == 6U)
        old_config = RGMII_U32(0U);

    if (speed == 3U) {
        if (duplex != 0U)
            new_config = (old_config & ~0x8000U) | 0x2000U;
        else
            new_config = old_config & ~0xa000U;
    } else {
        if (duplex != 0U)
            new_config = old_config | 0xa000U;
        else
            new_config = (old_config & ~0x2000U) | 0x8000U;

        if (speed == 2U)
            new_config |= 0x4000U;
        else
            new_config &= ~0x4000U;
    }

    if (mac <= initial_max_index)
        NPPT_U32(SMAC_REGISTER_BLOCK(mac)) = new_config;
    if (mac == 6U)
        RGMII_U32(0U) = new_config;

    smac_mask = 1U << mac;
    if ((new_config & 0x2000U) != 0U)
        NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) &= ~smac_mask;
    else
        NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) |= smac_mask;

    if ((new_config & 0x2000U) == (old_config & 0x2000U))
        return;

    printk("switch_smac_config_speed_duplex: duplex mode changed\n");
    if (isCpuType_133() == 1) {
        auto_gate_was_enabled = (uint32_t)greg_sopc_auto_gate_en_get();
        if (auto_gate_was_enabled == 1U)
            greg_sopc_auto_gate_en_set(0U);
    }

    smac_reset(smac_mask);
    NPPT_U32(SMAC_REGISTER_BLOCK(mac)) = new_config;
    printk("smac_change_duplex_init: smac_mac_cfg = 0x%x, mac = 0x%x\n",
           new_config, mac);

    if (mac <= g_smac_max_index) {
        uint32_t packet_filter;

        if ((new_config & 0x2000U) != 0U)
            NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) &= ~smac_mask;
        else
            NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) |= smac_mask;

        NPPT_U32(SMAC_REGISTER_BLOCK(mac) + 8U) = 0x80000001U;
        packet_filter = NPPT_U32(SMAC_REGISTER_BLOCK(mac) + 8U);
        printk("smac_change_duplex_init: smac_pkt_filter = 0x%x, mac = 0x%x\n",
               packet_filter, mac);
        NPPT_U32(SMAC_REGISTER_BLOCK(mac) + 0xe0U) = 0x00011200U;
        NPPT_U32(SMAC_REGISTER_BLOCK(mac) + 0xd00U) |= 2U;
        NPPT_U32(SMAC_REGISTER_BLOCK(mac) + 0xd30U) |= 0x20U;
    } else if (mac == 6U) {
        uint32_t packet_filter;

        if ((new_config & 0x2000U) != 0U)
            NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) &= ~smac_mask;
        else
            NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) |= smac_mask;

        RGMII_U32(8U) = 0x80000001U;
        packet_filter = RGMII_U32(8U);
        printk("smac_change_duplex_init: smac_pkt_filter = 0x%x, mac = 0x%x\n",
               packet_filter, 6U);
        RGMII_U32(0xe0U) = 0x00011200U;
        RGMII_U32(0xd00U) |= 2U;
        RGMII_U32(0xd30U) |= 0x20U;
    }

    sopc_send_enable(mac);
    printk("sopc send enable ,mac = 0x%x\n", mac);
    if (auto_gate_was_enabled == 1U && isCpuType_133() == 1)
        greg_sopc_auto_gate_en_set(1U);
}

void nppt_smac_disable(uint8_t mac)
{
    uint32_t config_value = 0;
    uint32_t initial_max_index;
    uint32_t smac_mask;

    if (mac > 6U)
        return;

    initial_max_index = g_smac_max_index;
    if (mac <= initial_max_index) {
        config_value = NPPT_U32(SMAC_REGISTER_BLOCK(mac));
        config_value &= ~3U;
        NPPT_U32(SMAC_REGISTER_BLOCK(mac)) = config_value;
    } else if (mac == 6U) {
        config_value = RGMII_U32(0U);
        config_value &= ~3U;
        RGMII_U32(0U) = config_value;
    } else {
        xmac_tx_rx_disable((unsigned int)mac - 4U);
    }

    if (mac > g_smac_max_index)
        return;

    smac_mask = 1U << mac;
    if ((config_value & 0x2000U) != 0U)
        NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) &= ~smac_mask;
    else
        NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) |= smac_mask;
}

void nppt_smac_enable(uint8_t mac)
{
    uint32_t config_value = 0;
    uint32_t initial_max_index;
    uint32_t smac_mask;

    if (mac > 6U)
        return;

    initial_max_index = g_smac_max_index;
    if (mac <= initial_max_index) {
        config_value = NPPT_U32(SMAC_REGISTER_BLOCK(mac));
        config_value |= 3U;
        NPPT_U32(SMAC_REGISTER_BLOCK(mac)) = config_value;
    } else if (mac == 6U) {
        config_value = RGMII_U32(0U);
        config_value |= 3U;
        RGMII_U32(0U) = config_value;
    } else {
        xmac_tx_rx_enable((unsigned int)mac - 4U);
    }

    if (mac == g_smac_max_index) {
        smac_mask = 1U << mac;
        if ((config_value & 0x2000U) != 0U)
            NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) &= ~smac_mask;
        else
            NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) |= smac_mask;
    } else if (mac == 6U) {
        if ((config_value & 0x2000U) != 0U)
            NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) &= ~0x40U;
        else
            NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) |= 0x40U;
    }
}

void smac_thread_init(void)
{
    struct zte_task *task;

    task = kthread_create_on_node(smac_check_phy_task_thread, 0, -1,
                                  "smac_check_phy_task");
    if ((uintptr_t)task > (uintptr_t)-4096) {
        printk("create smac_check_phy_task_thread failed!\n");
        return;
    }

    wake_up_process(task);
    printk("create smac_check_phy_task_thread ok!\n");
}

int smac_check_phy_task_thread(void *argument)
{
    int should_stop;

    (void)argument;
    for (;;) {
        should_stop = kthread_should_stop();
        if ((should_stop & 0xff) != 0)
            return should_stop;

        check_phy(0U);
        check_phy(1U);
        check_phy(2U);
        check_phy(3U);
        check_phy(4U);
        check_phy(5U);
        check_phy(6U);
        msleep_interruptible(100U);
    }
}

void check_phy(uint8_t mac)
{
    int status;
    uint8_t xmac_slot;
    unsigned int duplex;
    zte_smac_check_phy_t phy_check;

    if (check_phy_en == 1U)
        return;

    phy_check = sg_smac_check_phy[mac];
    if (phy_check == 0)
        return;

    status = phy_check(uni_phy[mac]);
    if (status == uni_phy_stat[mac])
        return;

    xmac_slot = (uint8_t)(mac - 4U);
    if (status == -1) {
        printk("mac %d link down\n", mac);
        nppt_smac_disable(mac);
        if (xmac_slot <= 1U)
            xmac_set_pcs_for_sgmii_half_duplex(xmac_slot, 0U, 3U, 1U);
    } else {
        duplex = ((uint32_t)status >> 10) & 1U;
        if ((uint8_t)status <= 6U)
            printk("mac %d phy status changed: %s %s\n", mac,
                   phy_speed[(uint8_t)status], phy_duplex[duplex]);

        if (xmac_slot > 1U) {
            nppt_smac_config_speed_duplex(mac, status, duplex);
        } else if ((isCpuType_133() == 1 || isCpuType_129() == 1) &&
                   (g_xmac0_type != 0U || mac != 4U) &&
                   (g_xmac1_type != 0U || mac != 5U)) {
            xmac_config_speed_duplex(xmac_slot, (uint8_t)status, duplex);
            if (duplex == 0U)
                xmac_set_pcs_for_sgmii_half_duplex(xmac_slot, 1U,
                                                    (uint8_t)status, 1U);
        }
        nppt_smac_enable(mac);
    }

    uni_phy_stat[mac] = status;
}

void xmac_eee_conf(uint8_t xmac, uint8_t enable)
{
    volatile uint32_t *control_register;
    volatile uint32_t *timing_register;
    uint32_t control_value;

    if (xmac == 2U || xmac == 3U) {
        control_register = (volatile uint32_t *)(uintptr_t)(
            (((uint32_t)xmac + 7U) << 16) + 0xd0U);
        timing_register = (volatile uint32_t *)(uintptr_t)(
            (((uint32_t)xmac + 7U) << 16) + 0xd4U);
    } else {
        control_register = (volatile uint32_t *)(nppt_base +
            ((uintptr_t)xmac << 18) + 0x140340U);
        timing_register = (volatile uint32_t *)(nppt_base +
            ((uintptr_t)xmac << 18) + 0x140350U);
    }

    control_value = *control_register;
    if (enable == 1U) {
        *control_register = control_value | 0x000b0000U;
        *timing_register = 0x03e80020U;
    } else {
        *control_register = control_value & 0xfff4ffffU;
        *timing_register = 0x03e80000U;
    }
}

void xmac0_wan_port_sel(uint8_t selection)
{
    *(volatile uint32_t *)(sys_ctrl_base + 0x0f4U) = selection;
}

void xmac_status_show(void)
{
    char mode_names[11][64] = {
        [0] = "10G BASE-R",
        [1] = "5G BASE-R",
        [2] = "1000BASE-X",
        [3] = "SGMII",
        [4] = "2.5G BASE-X",
        [5] = "10G USXGMII",
        [6] = "5G USXGMII",
        [7] = "2.5G USXGMII",
        [8] = "HSGMII",
        [9] = "NONE",
    };

    printk("xmac 0:\n");
    printk("  work mode:        %s\n",
           mode_names[(uint32_t)sg_xmac_work_mode[0]]);
    printk("  auto-negotiation: %d\n", g_xmac_work_in_auto[0]);
    printk("xmac 1:\n");
    printk("  work mode:        %s\n",
           mode_names[(uint32_t)sg_xmac_work_mode[1]]);
    printk("  auto-negotiation: %d\n", g_xmac_work_in_auto[1]);
}

void phy_dynamic_identify(void)
{
    if (kallsyms_lookup_name("aal_phy_enable_set") != 0) {
        g_phy_type = 0;
        return;
    }

    if (kallsyms_lookup_name("phy_common_c45_enable_set") != 0)
        g_phy_type = 1;
}

void xmac_set_speed_sel(uint8_t xmac, unsigned int speed)
{
    volatile uint32_t *speed_select_register;
    uint32_t value;

    if (xmac == 2U || xmac == 3U) {
        speed_select_register =
            (volatile uint32_t *)(uintptr_t)((xmac + 7U) << 16);
    } else {
        speed_select_register = (volatile uint32_t *)(nppt_base +
            ((uintptr_t)xmac << 18) + 0x140000U);
    }

    value = *speed_select_register & 0x1fffffffU;
    *speed_select_register = value | (speed << 29);
}

void xmac_set_duplex_mode(uint8_t xmac, unsigned int duplex)
{
    volatile uint32_t *duplex_register;
    uint32_t value;

    if (xmac == 2U || xmac == 3U) {
        duplex_register = (volatile uint32_t *)(uintptr_t)
            (((xmac + 7U) << 16) + 0x140U);
    } else {
        duplex_register = (volatile uint32_t *)(nppt_base +
            ((uintptr_t)xmac << 18) + 0x140500U);
    }

    value = *duplex_register & 0xfeffffffU;
    if (duplex == 0U)
        value |= 0x01000000U;
    *duplex_register = value;
}

void xmac_get_nppt_glb_xpcs_speed_duplex_in_sgmii_mode(
    uint8_t xmac, uint32_t *speed, uint32_t *duplex, uint32_t *auto_status)
{
    uint32_t status = *(volatile uint32_t *)(nppt_base + 0x090U +
                                              4U * (uint32_t)xmac);

    *speed = status & 7U;
    *duplex = (status >> 3) & 1U;
    *auto_status = (status >> 4) & 1U;
}

void xmac_get_nppt_glb_xpcs_speed_duplex_in_usxgmii_mode(
    uint8_t xmac, uint32_t *speed, uint32_t *duplex, uint32_t *auto_status)
{
    xmac_get_nppt_glb_xpcs_speed_duplex_in_sgmii_mode(xmac, speed, duplex,
                                                       auto_status);
}

void xmac_get_duplex_mode(uint8_t xmac, unsigned int *duplex)
{
    volatile uint32_t *duplex_register;

    if (xmac == 2U || xmac == 3U) {
        duplex_register = (volatile uint32_t *)(uintptr_t)
            (((xmac + 7U) << 16) + 0x140U);
    } else {
        duplex_register = (volatile uint32_t *)(nppt_base +
            ((uintptr_t)xmac << 18) + 0x140500U);
    }

    if ((*duplex_register & 0x01000000U) != 0U)
        *duplex = 0U;
    else
        *duplex = 1U;
}

void xmac_get_nppt_glb_link_status(unsigned int xmac, int *link_up)
{
    *link_up = (int)((NPPT_U32(0x84U) >> (xmac & 31U)) & 1U);
}

void xmac_get_uni_speed_from_xmac(uint8_t xmac, int *speed)
{
    volatile uint32_t *speed_select_register;
    uint32_t speed_select;

    if (xmac == 2U || xmac == 3U) {
        speed_select_register =
            (volatile uint32_t *)(uintptr_t)((xmac + 7U) << 16);
    } else {
        speed_select_register = (volatile uint32_t *)(nppt_base +
            ((uintptr_t)xmac << 18) + 0x140000U);
    }

    speed_select = *speed_select_register >> 29;
    *speed = xmac_speed_select_to_uni_speed[speed_select];
}

void xamc_init_conf_by_speed(uint8_t xmac, unsigned int speed)
{
    volatile uint8_t *register_base;
    uintptr_t config_1_offset;
    uintptr_t config_2_offset;
    uintptr_t config_3_offset;
    uintptr_t config_4_offset;
    uint32_t value;

    if (xmac == 2U || xmac == 3U) {
        register_base =
            (volatile uint8_t *)(uintptr_t)((xmac + 7U) << 16);
        config_1_offset = 0x4U;
        config_2_offset = 0x8U;
        config_3_offset = 0xa0U;
        config_4_offset = 0xd00U;
    } else {
        register_base = nppt_base + ((uintptr_t)xmac << 18) + 0x140000U;
        config_1_offset = 0x10U;
        config_2_offset = 0x20U;
        config_3_offset = 0x280U;
        config_4_offset = 0x3400U;
    }

    *(volatile uint32_t *)register_base = 0x00010000U;
    xmac_set_speed_sel(xmac, speed);
    xmac_set_duplex_mode(xmac, 1U);
    *(volatile uint32_t *)(register_base + config_1_offset) = 0x3e800086U;
    *(volatile uint32_t *)(register_base + config_2_offset) = 0x80000001U;
    *(volatile uint32_t *)(register_base + config_3_offset) = 2U;
    value = *(volatile uint32_t *)(register_base + config_4_offset);
    *(volatile uint32_t *)(register_base + config_4_offset) = value | 0x200U;
}

void xamc_init_conf(uint8_t xmac)
{
    volatile uint8_t *register_base;
    uintptr_t config_1_offset;
    uintptr_t config_2_offset;
    uintptr_t config_3_offset;
    uintptr_t config_4_offset;
    uint32_t value;

    if (xmac == 2U || xmac == 3U) {
        register_base =
            (volatile uint8_t *)(uintptr_t)((xmac + 7U) << 16);
        config_1_offset = 0x4U;
        config_2_offset = 0x8U;
        config_3_offset = 0xa0U;
        config_4_offset = 0xd00U;
    } else {
        register_base = nppt_base + ((uintptr_t)xmac << 18) + 0x140000U;
        config_1_offset = 0x10U;
        config_2_offset = 0x20U;
        config_3_offset = 0x280U;
        config_4_offset = 0x3400U;
    }

    *(volatile uint32_t *)register_base = 0x00010000U;
    *(volatile uint32_t *)(register_base + config_1_offset) = 0x3e800086U;
    *(volatile uint32_t *)(register_base + config_2_offset) = 0x80000001U;
    *(volatile uint32_t *)(register_base + config_3_offset) = 2U;
    value = *(volatile uint32_t *)(register_base + config_4_offset);
    *(volatile uint32_t *)(register_base + config_4_offset) = value | 0x200U;
}

void xmac_reset(uint8_t xmac)
{
    smac_reset(xmac == 0U ? 0x400U : 0x800U);
}

void xmac_tx_disable(unsigned int xmac)
{
    volatile uint32_t *tx_register;

    if (xmac == 2U || xmac == 3U) {
        tx_register = (volatile uint32_t *)(uintptr_t)
            (((uint16_t)(xmac + 7U)) << 16);
    } else {
        tx_register = (volatile uint32_t *)(nppt_base +
            ((uintptr_t)(xmac & 0x3fffU) << 18) + 0x140000U);
    }

    *tx_register &= ~1U;
}

void xmac_tx_enable(unsigned int xmac)
{
    volatile uint32_t *tx_register;

    if (xmac == 2U || xmac == 3U) {
        tx_register = (volatile uint32_t *)(uintptr_t)
            (((uint16_t)(xmac + 7U)) << 16);
    } else {
        tx_register = (volatile uint32_t *)(nppt_base +
            ((uintptr_t)(xmac & 0x3fffU) << 18) + 0x140000U);
    }

    *tx_register |= 1U;
}

void xmac_rx_enable(unsigned int xmac)
{
    volatile uint32_t *rx_register;

    if (xmac == 2U || xmac == 3U) {
        rx_register = (volatile uint32_t *)(uintptr_t)
            ((((uint16_t)(xmac + 7U)) << 16) + 4U);
    } else {
        rx_register = (volatile uint32_t *)(nppt_base +
            ((uintptr_t)(xmac & 0x3fffU) << 18) + 0x140010U);
    }

    *rx_register |= 1U;
}

void xmac_rx_disable(unsigned int xmac)
{
    volatile uint32_t *rx_register;

    if (xmac == 2U || xmac == 3U) {
        rx_register = (volatile uint32_t *)(uintptr_t)
            ((((uint16_t)(xmac + 7U)) << 16) + 4U);
    } else {
        rx_register = (volatile uint32_t *)(nppt_base +
            ((uintptr_t)(xmac & 0x3fffU) << 18) + 0x140010U);
    }

    *rx_register &= ~1U;
}

void xmac_tx_rx_enable(unsigned int xmac)
{
    xmac_rx_enable(xmac);
    xmac_tx_enable(xmac);
}

void xmac_tx_rx_disable(unsigned int xmac)
{
    xmac_rx_disable(xmac);
    xmac_tx_disable(xmac);
}

void xmac_rlt_phy_init(void)
{
    if (isCpuType_133() != 1U)
        (void)isCpuType_129();
}

void xmac_mvl_phy_init(void)
{
    if ((isCpuType_133() == 1U || isCpuType_129() == 1U) &&
        g_xmac0_type == 2U)
        return;

    (void)isCpuType_133();
}

void xmac_bcm_phy_init(void)
{
}

void xmac_aqr_phy_init(void)
{
    if (isCpuType_133() != 1U)
        (void)isCpuType_129();
}

void xmac_zxic_phy_init(void)
{
    unsigned int index;

    if (g_xmac0_type != 4U && g_xmac1_type != 4U)
        return;

    for (index = 0U; index != 3U; ++index) {
        uint8_t phy = (uint8_t)(index + 4U);

        if (phy_zxic051_port_exist(phy) == 0U)
            continue;

        (void)phy_zxic051_para_init(phy);
        xphy_check_callbacks[index] = (uintptr_t)phy_zxic051_check;
        sg_xphy_enable_set[index] = (uintptr_t)phy_zxic051_set_enable;
        sg_xphy_enable_get[index] = (uintptr_t)phy_zxic051_get_enable;
        sg_xphy_linkstatus_get[index] = (uintptr_t)phy_zxic051_get_linkstate;
        sg_xphy_linkmode_set[index] = (uintptr_t)phy_zxic051_set_linkmode;
        sg_xphy_linkmode_get[index] = (uintptr_t)phy_zxic051_get_linkmode;
        sg_xphy_loopback_set[index] = (uintptr_t)phy_zxic051_set_loopback;
        sg_xphy_loopback_get[index] = (uintptr_t)phy_zxic051_get_loopback;
    }
}

void xmac_jl_phy_init(void)
{
}

int xmac_init(unsigned int xmac0_work_mode, unsigned int xmac1_work_mode)
{
    unsigned int status = 0;

    if (xmac_phy_type() != 9) {
        phy_dynamic_identify();
        xmac_rlt_phy_init();
        xmac_mvl_phy_init();
        xmac_aqr_phy_init();
        xmac_zxic_phy_init();
    }

    if (xmac_need_set_work_mode != 0)
        status = (unsigned int)xmac_init_by_work_mode(0U, xmac0_work_mode);
    if (dword_2677C != 0)
        status |= (unsigned int)xmac_init_by_work_mode(1U, xmac1_work_mode);

    if (status != 0)
        printk("xmac_init fail. xmac0_work_mode %d, xmac1_work_mode %d\n",
               xmac0_work_mode, xmac1_work_mode);
    if (g_ponserdes_to_xmac1 != 0)
        printk("xmac1_work_mode %d\n", xmac1_work_mode);

    return (int)status;
}

int xmac_init_by_work_mode(uint8_t xmac, unsigned int work_mode)
{
    int status;

    xpcs_init(xmac);
    xmac_reset(xmac);

    switch (work_mode) {
    case 0:
        status = xmac_10gbase_r_conf(xmac);
        break;
    case 1:
        status = xmac_5gbase_r_conf(xmac);
        break;
    case 2:
        status = xmac_1gbase_x_conf(xmac);
        break;
    case 3:
        status = xmac_sgmii_conf(xmac, 0U, 3U, 1U);
        break;
    case 4:
        status = xmac_2pt5gbase_x_conf(xmac);
        break;
    case 5:
        status = xmac_10g_usxgmii_auto_conf(xmac);
        break;
    case 6:
        status = xmac_5g_usxgmii_auto_conf(xmac);
        break;
    case 7:
        status = xmac_2pt5g_usxgmii_auto_conf(xmac);
        break;
    case 8:
        status = xmac_hsgmii_conf(xmac, 1U);
        break;
    case 9:
        status = xmac_hsgmii_conf(xmac, 0U);
        break;
    default:
        printk("unspport work mode %d\n", work_mode);
        return -1;
    }

    xmac_set_duplex_mode(xmac, 1U);
    xmac_set_sopc_duplex_mode(xmac, 1U);
    __const_udelay(0x418958UL);
    NPPT_U32(XMAC_SOPC_ENABLE_REGISTER(xmac)) = 1U;
    xmac_tx_rx_enable(xmac);

    if (status != 0) {
        printk("xmac %d work mode %d fail\n", xmac, work_mode);
        return status;
    }

    sg_xmac_work_mode[xmac] = (int32_t)work_mode;
    printk("xmac %d work mode %d\n", xmac, work_mode);
    return 0;
}

int xmac_10gbase_r_conf(uint8_t xmac)
{
    int status;

    if (xmac > 4U) {
        printk("xmac_id(%d) is error\n", xmac);
        return -1;
    }

    xmac_tx_disable(xmac);
    xmac_rx_disable(xmac);
    if (xmac <= 1U && isCpuType_132() == 1) {
        status = xpcs_10gbase_r_conf(xmac);
        xamc_init_conf_by_speed(xmac, 0U);
        __const_udelay(0x8312b0UL);
        status |= uni_serdes_init(xmac, 0U);
    } else {
        uni_serdes_init(xmac, 0U);
        __const_udelay(0x8312b0UL);
        status = xpcs_10gbase_r_conf(xmac);
        xamc_init_conf_by_speed(xmac, 0U);
        if (xmac <= 1U && isCpuType_133() == 1)
            byPassEnableSet(xmac, 1U);
    }

    sg_xmac_work_mode[xmac] = 0;
    return status;
}

int xmac_5gbase_r_conf(uint8_t xmac)
{
    int status;

    if (xmac > 4U) {
        printk("xmac_id(%d) is error\n", xmac);
        return -1;
    }

    xmac_tx_disable(xmac);
    xmac_rx_disable(xmac);
    if (xmac <= 1U && isCpuType_132() == 1) {
        status = xpcs_5gbase_r_conf(xmac);
        xamc_init_conf_by_speed(xmac, 5U);
        __const_udelay(0x8312b0UL);
        status |= uni_serdes_init(xmac, 2U);
    } else {
        uni_serdes_init(xmac, 2U);
        __const_udelay(0x8312b0UL);
        status = xpcs_5gbase_r_conf(xmac);
        xamc_init_conf_by_speed(xmac, 5U);
        if (xmac <= 1U && isCpuType_133() == 1)
            byPassEnableSet(xmac, 1U);
    }

    sg_xmac_work_mode[xmac] = 1;
    return status;
}

int xmac_1gbase_x_conf(uint8_t xmac)
{
    int status;

    if (xmac > 4U) {
        printk("xmac_id(%d) is error\n", xmac);
        return -1;
    }

    xmac_tx_disable(xmac);
    xmac_rx_disable(xmac);
    if (xmac <= 1U && isCpuType_132() == 1) {
        status = xpcs_1000base_x_conf(xmac, 3U, 1U);
        xamc_init_conf_by_speed(xmac, 3U);
        __const_udelay(0x8312b0UL);
        status |= uni_serdes_init(xmac, 7U);
    } else {
        uni_serdes_init(xmac, 7U);
        __const_udelay(0x8312b0UL);
        status = xpcs_1000base_x_conf(xmac, 3U, 1U);
        xamc_init_conf_by_speed(xmac, 3U);
        if (xmac <= 1U &&
            (isCpuType_133() == 1 || isCpuType_129() == 1))
            byPassEnableSet(xmac, 1U);
    }

    sg_xmac_work_mode[xmac] = 2;
    return status;
}

int xmac_sgmii_conf(uint8_t xmac, unsigned int auto_negotiation,
                    unsigned int mode_value, unsigned int config_value)
{
    int status;

    if (xmac > 4U) {
        printk("xmac_id(%d) is error\n", xmac);
        return -1;
    }

    auto_negotiation = (uint8_t)auto_negotiation;
    xmac_tx_disable(xmac);
    xmac_rx_disable(xmac);
    if (xmac <= 1U && isCpuType_132() == 1) {
        status = xpcs_sgmii_mode_conf(xmac, mode_value, config_value);
        status |= xpcs_auto_negotiation_conf_in_sgmii_mode(
            xmac, (uint8_t)auto_negotiation);
        xamc_init_conf_by_speed(xmac, 3U);
        __const_udelay(0x8312b0UL);
        status |= uni_serdes_init(xmac, 7U);
    } else {
        uni_serdes_init(xmac, 7U);
        __const_udelay(0x8312b0UL);
        status = xpcs_sgmii_mode_conf(xmac, mode_value, config_value);
        status |= xpcs_auto_negotiation_conf_in_sgmii_mode(
            xmac, (uint8_t)auto_negotiation);
        xamc_init_conf_by_speed(xmac, 3U);
        if (xmac <= 1U &&
            (isCpuType_133() == 1 || isCpuType_129() == 1))
            byPassEnableSet(xmac, 1U);
    }

    sg_xmac_work_mode[xmac] = 3;
    return status;
}

int xmac_2pt5gbase_x_conf(uint8_t xmac)
{
    int status;

    if (xmac > 4U) {
        printk("xmac_id(%d) is error\n", xmac);
        return -1;
    }

    xmac_tx_disable(xmac);
    xmac_rx_disable(xmac);
    if (xmac <= 1U && isCpuType_132() == 1) {
        status = xpcs_2p5gbase_x_conf(xmac);
        xamc_init_conf_by_speed(xmac, 6U);
        __const_udelay(0x8312b0UL);
        status |= uni_serdes_init(xmac, 5U);
    } else {
        uni_serdes_init(xmac, 5U);
        __const_udelay(0x8312b0UL);
        status = xpcs_2p5gbase_x_conf(xmac);
        xamc_init_conf_by_speed(xmac, 6U);
        if (xmac <= 1U &&
            (isCpuType_133() == 1 || isCpuType_129() == 1))
            byPassEnableSet(xmac, 1U);
    }

    sg_xmac_work_mode[xmac] = 4;
    return status;
}

int xmac_10g_usxgmii_auto_conf(uint8_t xmac)
{
    int status;

    if (xmac > 4U) {
        printk("xmac_id(%d) is error\n", xmac);
        return -1;
    }

    xmac_tx_disable(xmac);
    xmac_rx_disable(xmac);
    if (xmac <= 1U && isCpuType_132() == 1) {
        status = xpcs_usxgmii_mode_conf(xmac, 0U);
        status |= xpcs_auto_negotiation_conf_in_usxgmii_mode(xmac, 1U);
        xamc_init_conf_by_speed(xmac, 0U);
        __const_udelay(0x8312b0UL);
        status |= uni_serdes_init(xmac, 1U);
    } else {
        uni_serdes_init(xmac, 1U);
        __const_udelay(0x8312b0UL);
        status = xpcs_usxgmii_mode_conf(xmac, 0U);
        status |= xpcs_auto_negotiation_conf_in_usxgmii_mode(xmac, 1U);
        xamc_init_conf_by_speed(xmac, 0U);
        if (xmac <= 1U && isCpuType_133() == 1)
            byPassEnableSet(xmac, 1U);
    }

    sg_xmac_work_mode[xmac] = 5;
    return status;
}

int xmac_5g_usxgmii_auto_conf(uint8_t xmac)
{
    int status;

    if (xmac > 4U) {
        printk("xmac_id(%d) is error\n", xmac);
        return -1;
    }

    xmac_tx_disable(xmac);
    xmac_rx_disable(xmac);
    if (xmac <= 1U && isCpuType_132() == 1) {
        status = xpcs_usxgmii_mode_conf(xmac, 1U);
        status |= xpcs_auto_negotiation_conf_in_usxgmii_mode(xmac, 1U);
        xamc_init_conf_by_speed(xmac, 5U);
        __const_udelay(0x8312b0UL);
        status |= uni_serdes_init(xmac, 3U);
    } else {
        uni_serdes_init(xmac, 3U);
        __const_udelay(0x8312b0UL);
        status = xpcs_usxgmii_mode_conf(xmac, 1U);
        status |= xpcs_auto_negotiation_conf_in_usxgmii_mode(xmac, 1U);
        xamc_init_conf_by_speed(xmac, 5U);
        if (xmac <= 1U && isCpuType_133() == 1)
            byPassEnableSet(xmac, 1U);
    }

    sg_xmac_work_mode[xmac] = 6;
    return status;
}

int xmac_2pt5g_usxgmii_auto_conf(uint8_t xmac)
{
    int status;

    if (xmac > 4U) {
        printk("xmac_id(%d) is error\n", xmac);
        return -1;
    }

    xmac_tx_disable(xmac);
    xmac_rx_disable(xmac);
    if (xmac <= 1U && isCpuType_132() == 1) {
        status = xpcs_usxgmii_mode_conf(xmac, 2U);
        status |= xpcs_auto_negotiation_conf_in_usxgmii_mode(xmac, 1U);
        xamc_init_conf_by_speed(xmac, 6U);
        __const_udelay(0x8312b0UL);
        status |= uni_serdes_init(xmac, 4U);
    } else {
        uni_serdes_init(xmac, 4U);
        __const_udelay(0x8312b0UL);
        status = xpcs_usxgmii_mode_conf(xmac, 2U);
        status |= xpcs_auto_negotiation_conf_in_usxgmii_mode(xmac, 1U);
        xamc_init_conf_by_speed(xmac, 6U);
        if (xmac <= 1U &&
            (isCpuType_133() == 1 || isCpuType_129() == 1))
            byPassEnableSet(xmac, 1U);
    }

    sg_xmac_work_mode[xmac] = 7;
    return status;
}

int xmac_hsgmii_conf(uint8_t xmac, unsigned int variant)
{
    int status;

    if (xmac > 4U) {
        printk("xmac_id(%d) is error\n", xmac);
        return -1;
    }

    variant = (uint8_t)variant;
    xmac_tx_disable(xmac);
    xmac_rx_disable(xmac);
    if (xmac <= 1U && isCpuType_132() == 1) {
        status = xpcs_hsgmii_mode_conf(xmac, (uint8_t)variant);
        xamc_init_conf_by_speed(xmac, 2U);
        __const_udelay(0x8312b0UL);
        status |= uni_serdes_init(xmac, 5U);
    } else {
        uni_serdes_init(xmac, 5U);
        __const_udelay(0x8312b0UL);
        status = xpcs_hsgmii_mode_conf(xmac, (uint8_t)variant);
        xamc_init_conf_by_speed(xmac, 2U);
        if (xmac <= 1U &&
            (isCpuType_133() == 1 || isCpuType_129() == 1))
            byPassEnableSet(xmac, 1U);
    }

    sg_xmac_work_mode[xmac] = 8;
    return status;
}

int xmac_test_siwtch_work_mode(uint8_t xmac, int work_mode)
{
    int status;

    switch (work_mode) {
    case 0:
        status = xmac_10gbase_r_conf(xmac);
        printk("work in 10GBASE-R mode. ret = %d\n", status);
        break;
    case 1:
        status = xmac_5gbase_r_conf(xmac);
        printk("work in 5GBASE-R mode. ret = %d\n", status);
        break;
    case 2:
        status = xmac_1gbase_x_conf(xmac);
        printk("work in 1000BASE-X mode. ret = %d\n", status);
        break;
    case 3:
        status = xmac_sgmii_conf(xmac, 0U, 3U, 1U);
        printk("work in SGMII 1G FORCE mode. ret = %d\n", status);
        break;
    case 4:
        status = xmac_2pt5gbase_x_conf(xmac);
        printk("work in 2.5GBASE-R mode. ret = %d\n", status);
        break;
    case 5:
        status = xmac_10g_usxgmii_auto_conf(xmac);
        printk("work in 10G USXGMII AUTO mode. ret = %d\n", status);
        break;
    case 6:
        status = xmac_5g_usxgmii_auto_conf(xmac);
        printk("work in 5G USXGMII AUTO mode. ret = %d\n", status);
        break;
    case 7:
        status = xmac_2pt5g_usxgmii_auto_conf(xmac);
        printk("work in 2.5G USXGMII AUTO mode. ret = %d\n", status);
        break;
    case 8:
        status = xmac_hsgmii_conf(xmac, 1U);
        printk("work in HSGMII AUTO mode. ret = %d\n", status);
        break;
    case 9:
        status = xmac_hsgmii_conf(xmac, 0U);
        printk("work in HSGMII FORCE mode. ret = %d\n", status);
        break;
    default:
        printk("unspport work mode\n");
        return 0;
    }

    xmac_tx_rx_enable(xmac);
    return status;
}

int xmac_work_mode_switch_to_serdes_mode(int work_mode, int *serdes_mode)
{
    switch (work_mode) {
    case 0:
        *serdes_mode = 0;
        return 0;
    case 1:
        *serdes_mode = 2;
        return 0;
    case 2:
    case 3:
        *serdes_mode = 7;
        return 0;
    case 4:
    case 7:
        *serdes_mode = 4;
        return 0;
    case 5:
        *serdes_mode = 1;
        return 0;
    case 6:
        *serdes_mode = 3;
        return 0;
    case 8:
        *serdes_mode = 5;
        return 0;
    default:
        printk("unspport xmac work mode %d\n", work_mode);
        return -1;
    }
}

int xmac_mode_set(uint8_t xmac, unsigned int pcs_mode,
                  unsigned int input_speed, uint8_t config_value)
{
    unsigned int speed = 1U;
    unsigned int speed_index = (uint8_t)(input_speed - 1U);
    int status;

    if (pcs_mode > 12U) {
        printk("ERROR: [outerphy_set_xmac_mode] can't find this mode <%x>\n",
               pcs_mode);
        return -1;
    }

    if (speed_index <= 5U)
        speed = xmac_mode_speed_select[speed_index];

    switch (pcs_mode) {
    case 0:
        status = xmac_10gbase_r_conf(xmac);
        speed = 0U;
        break;
    case 1:
        status = xmac_5gbase_r_conf(xmac);
        speed = 5U;
        break;
    case 2:
        status = xmac_1gbase_x_conf(xmac);
        speed = 3U;
        break;
    case 3:
        status = xmac_2pt5gbase_x_conf(xmac);
        speed = 2U;
        break;
    case 4:
        return xmac_hsgmii_conf(xmac, 0U);
    case 5:
        status = xmac_sgmii_conf(xmac, 0U, speed, config_value);
        break;
    case 6:
        status = xmac_sgmii_conf(xmac, 1U, 3U, 1U);
        speed = 1U;
        break;
    case 7:
        status = xmac_10g_usxgmii_auto_conf(xmac);
        speed = 1U;
        break;
    case 8:
        status = xmac_5g_usxgmii_auto_conf(xmac);
        speed = 1U;
        break;
    case 9:
        status = xmac_2pt5g_usxgmii_auto_conf(xmac);
        speed = 1U;
        break;
    default:
        printk("xmac doesn't support this pcs mode(%u) \n", pcs_mode);
        return -1;
    }

    xmac_set_speed_sel(xmac, speed);
    return status;
}

void xmac_switch_uni_speed_to_xmac_speed(uint8_t xmac,
                                         unsigned int uni_speed,
                                         unsigned int *xmac_speed)
{
    switch (uni_speed) {
    case 1:
        *xmac_speed = 7U;
        break;
    case 2:
        *xmac_speed = 4U;
        break;
    case 3:
        *xmac_speed = 3U;
        break;
    case 4:
        if ((uint32_t)sg_xmac_work_mode[xmac] - 8U <= 1U)
            *xmac_speed = 2U;
        else
            *xmac_speed = 6U;
        break;
    case 5:
        *xmac_speed = 5U;
        break;
    case 6:
        *xmac_speed = 0U;
        break;
    default:
        break;
    }
}

void xmac_speed_process_in_sgmii_auto_mode(uint8_t xmac)
{
    unsigned int uni_speed = 0;
    unsigned int duplex = 0;
    unsigned int auto_status = 0;
    unsigned int xmac_speed = 1;

    if (g_xmac_work_in_auto[xmac] == 0U ||
        sg_xmac_work_mode[xmac] != 3 ||
        xpcs_get_speed_duplex_in_auto_en_sgmii_mode(
            xmac, &uni_speed, &duplex, &auto_status) != 0 ||
        auto_status != 1U)
        return;

    xmac_switch_uni_speed_to_xmac_speed(xmac, uni_speed, &xmac_speed);
    xmac_set_speed_sel(xmac, xmac_speed);
    printk("xmac %d speed set %u in sgmii(xmac speed %d)\n", xmac,
           uni_speed, xmac_speed);
}

void xmac_config_speed_duplex(uint8_t xmac, unsigned int uni_speed,
                              unsigned int duplex)
{
    unsigned int xmac_speed = 1;
    unsigned int current_duplex = 2;
    int auto_gate_was_enabled = 0;

    xmac_switch_uni_speed_to_xmac_speed(xmac, uni_speed, &xmac_speed);
    xmac_get_duplex_mode(xmac, &current_duplex);
    if (current_duplex == duplex) {
        xmac_set_speed_sel(xmac, xmac_speed);
        return;
    }

    if (isCpuType_133() == 1) {
        auto_gate_was_enabled = greg_sopc_auto_gate_en_get();
        if (auto_gate_was_enabled == 1)
            greg_sopc_auto_gate_en_set(0U);
    }

    xmac_reset(xmac);
    xamc_init_conf_by_speed(xmac, xmac_speed);
    xmac_set_duplex_mode(xmac, duplex);
    xmac_set_sopc_duplex_mode(xmac, duplex);
    xmac_sopc_send_enable(xmac);

    if (auto_gate_was_enabled == 1 && isCpuType_133() == 1)
        greg_sopc_auto_gate_en_set(1U);
}

void xmac_set_sopc_duplex_mode(uint8_t xmac, unsigned int duplex)
{
    uint32_t value;

    if (xmac > 4U) {
        printk("xmac_id(%d) is error\n", xmac);
        return;
    }

    value = NPPT_U32(SMAC_SOPC_ENABLE_REGISTER);
    if (duplex == 1U)
        value &= ~(1U << (xmac + 4U));
    else
        value |= 1U << (xmac + 4U);
    NPPT_U32(SMAC_SOPC_ENABLE_REGISTER) = value;
}

void xmac_sopc_send_enable(uint8_t xmac)
{
    uint32_t ready;

    for (;;) {
        ready = NPPT_U32(XMAC_SOPC_READY_REGISTER(xmac));
        printk("read SOPC_CLR_OVER_READY_SMAC = 0x%x, mac = 0x%x\n",
               ready, xmac + 4U);
        if ((ready & 1U) != 0U)
            break;
        __const_udelay(0x418958UL);
    }

    __const_udelay(0x418958UL);
    NPPT_U32(XMAC_SOPC_ENABLE_REGISTER(xmac)) = 1U;
    printk("write SOPC_SEND_EN_CFG_SMAC,mac = 0x%x\n", xmac + 4U);
}

void xmac_speed_process_in_usxgmii_auto_mode(uint8_t xmac)
{
    unsigned int uni_speed = 0;
    unsigned int duplex = 0;
    unsigned int auto_status = 0;
    unsigned int xmac_speed = 1;

    if (g_xmac_work_in_auto[xmac] == 0U ||
        (uint32_t)sg_xmac_work_mode[xmac] - 5U > 2U ||
        xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode(
            xmac, &uni_speed, &duplex, &auto_status) != 0 ||
        auto_status != 1U)
        return;

    xmac_switch_uni_speed_to_xmac_speed(xmac, uni_speed, &xmac_speed);
    xmac_set_speed_sel(xmac, xmac_speed);
    printk("xmac %d speed set %u in usxgmii(xmac speed %d)\n", xmac,
           uni_speed, xmac_speed);
}

void xmac_speed_process(uint8_t xmac)
{
    int32_t work_mode;
    unsigned int uni_speed = 0;
    unsigned int duplex = 0;
    unsigned int auto_status = 0;
    unsigned int xmac_speed = 1;

    if (g_xmac_work_in_auto[xmac] == 0U)
        return;

    work_mode = sg_xmac_work_mode[xmac];
    if (work_mode == 3) {
        if (xpcs_get_speed_duplex_in_auto_en_sgmii_mode(
                xmac, &uni_speed, &duplex, &auto_status) != 0 ||
            auto_status != 1U)
            return;

        xmac_switch_uni_speed_to_xmac_speed(xmac, uni_speed, &xmac_speed);
        xmac_set_speed_sel(xmac, xmac_speed);
        printk("xmac %d speed set %u in sgmii(xmac speed %d)\n", xmac,
               uni_speed, xmac_speed);
    } else if ((uint32_t)work_mode - 5U <= 2U) {
        if (xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode(
                xmac, &uni_speed, &duplex, &auto_status) != 0 ||
            auto_status != 1U)
            return;

        xmac_switch_uni_speed_to_xmac_speed(xmac, uni_speed, &xmac_speed);
        xmac_set_speed_sel(xmac, xmac_speed);
        printk("xmac %d speed set %u in usxgmii(xmac speed %d)\n", xmac,
               uni_speed, xmac_speed);
    }
}

void xmac_set_pcs_for_sgmii_half_duplex(unsigned int xmac,
                                         unsigned int configure,
                                         unsigned int speed,
                                         unsigned int state)
{
    int32_t work_mode;
    uint8_t pcs_xmac;

    work_mode = sg_xmac_work_mode[xmac];
    if (work_mode != 3 || g_xmac_work_in_auto[xmac] == 0U)
        return;

    pcs_xmac = (uint8_t)xmac;
    if (configure == 1U) {
        xpcs_set_speed_duplex_in_sgmii_anto_disale_mode(pcs_xmac, speed,
                                                         state);
        xpcs_set_sr_mii_ctrl_an_enable(pcs_xmac, 0U);
        return;
    }

    if (xpcs_sr_mii_ctrl_is_an_enable(pcs_xmac) != 0U)
        return;

    xpcs_set_sr_mii_ctrl_speed(pcs_xmac, (unsigned int)work_mode);
    xpcs_set_sr_mii_ctrl_duplex_mode(pcs_xmac, 1U);
    xpcs_set_vr_mii_an_ctrl_sgmii_link_sts(pcs_xmac, 0U);
    xpcs_set_sr_mii_ctrl_an_enable(pcs_xmac, 1U);
}

void xpcs_set_speed_duplex_in_sgmii_anto_disale_mode(uint8_t xmac,
                                                      unsigned int speed,
                                                      unsigned int state)
{
    xpcs_set_sr_mii_ctrl_speed(xmac, speed);
    xpcs_set_sr_mii_ctrl_duplex_mode(xmac, state);
    xpcs_set_vr_mii_an_ctrl_sgmii_link_sts(xmac, 1U);
}

static volatile uint32_t *xpcs_register_for_xmac(uint8_t xmac,
                                                  uintptr_t offset)
{
    if (xmac == 2U || xmac == 3U) {
        return (volatile uint32_t *)(uintptr_t)
            ((((uintptr_t)xmac) << 23) + offset);
    }

    return (volatile uint32_t *)(xmac0_pcs_base +
        (intptr_t)(int32_t)((uint32_t)xmac << 24) + offset);
}

int byPassEnableSet(uint8_t xmac, uint8_t enable)
{
    volatile uint32_t *bypass_register;
    uint32_t bypass_value;

    if (isCpuType_133() != 1 && isCpuType_129() != 1)
        return 0;

    bypass_register = xpcs_register_for_xmac(xmac, 0x0e0014U);
    bypass_value = *bypass_register;
    if (enable != 0U)
        bypass_value |= 0x10U;
    else
        bypass_value &= 0xffffffefU;
    *bypass_register = bypass_value;
    return 0;
}

int phy_zx5201_check(uint8_t phy)
{
    uint16_t extended_status;
    unsigned int status_code;

    (void)zx_mdio_read_ge_ext(phy, 26U);
    extended_status = zx_mdio_read_ge_ext(phy, 26U);
    if ((extended_status & 0x40U) == 0U)
        return -1;

    switch ((extended_status >> 8) & 3U) {
    case 0U:
        status_code = 1U;
        break;
    case 1U:
        status_code = 2U;
        break;
    case 2U:
        status_code = 3U;
        break;
    default:
        status_code = 7U;
        break;
    }

    return (int)(status_code | (((extended_status >> 7) & 1U) << 10));
}

int phy_zx5201_init(uint8_t phy)
{
    uint8_t adjacent_phy;
    uint32_t register_21_value;
    uint16_t register_20_value;

    adjacent_phy = (uint8_t)(phy + 1U);
    zx_mdio_write_ge_ext(phy, 18U, 0xffff8402U);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(adjacent_phy, 22U, 0x0a07U);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(adjacent_phy, 27U, 0x0800U);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(phy, 29U, 0x0355U);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(phy, 16U, 0xffffb62dU);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(phy, 17U, 6U);
    __const_udelay(429500UL);

    zx_mdio_write_ge_ext(adjacent_phy, 18U, 4U);
    register_21_value = zx_mdio_read_ge_ext(adjacent_phy, 21U) & 0xffffc1ffU;
    register_20_value = zx_mdio_read_ge_ext(adjacent_phy, 20U);
    zx_mdio_write_ge_ext(adjacent_phy, 17U, register_21_value | 0x2800U);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(adjacent_phy, 16U, register_20_value);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(adjacent_phy, 18U, 0x0204U);
    __const_udelay(429500UL);
    return printk("rgmii phy init done\n");
}

int phy_8574_check(uint8_t phy)
{
    uint16_t saved_page;
    uint16_t status_register;
    unsigned int status_code;

    if ((zx_mdio_read_ge_ext(phy, 1U) & 4U) == 0U)
        return -1;

    saved_page = zx_mdio_read_ge_ext(phy, 31U);
    zx_mdio_write_ge_ext(phy, 31U, 0U);
    (void)zx_mdio_read_ge_ext(phy, 28U);
    status_register = zx_mdio_read_ge_ext(phy, 28U);
    zx_mdio_write_ge_ext(phy, 31U, saved_page);

    switch ((status_register >> 3) & 3U) {
    case 0U:
        status_code = 1U;
        break;
    case 1U:
        status_code = 2U;
        break;
    case 2U:
        status_code = 3U;
        break;
    default:
        status_code = 7U;
        break;
    }

    return (int)(status_code | (((status_register >> 5) & 1U) << 10));
}

int phy_8574_init(uint8_t phy)
{
    uint16_t status_register;
    unsigned int retries;
    uint16_t register_value;

    if (phy_8574_has_initialized == 0U) {
        printk("init_VSC8574 enable 4 ports sgmii!\n");
        zx_mdio_write_ge_ext(phy, 31U, 16U);
        zx_mdio_write_ge_ext(phy, 18U, 0xffff80f0U);
        status_register = zx_mdio_read_ge_ext(phy, 18U);
        retries = 1002U;
        while ((status_register & 0x8000U) == 0U) {
            retries = (uint16_t)(retries - 1U);
            status_register = zx_mdio_read_ge_ext(phy, 18U);
            if (retries == 0U) {
                printk("init_VSC8574 can not wait reg change!\n");
                break;
            }
        }
        phy_8574_has_initialized = 1U;
    }

    zx_mdio_write_ge_ext(phy, 31U, 3U);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(phy, 16U, 0x01a0U);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(phy, 31U, 0U);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(phy, 0U, 0xffff9040U);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(phy, 31U, 16U);
    __const_udelay(429500UL);
    register_value = zx_mdio_read_ge_ext(phy, 25U);
    zx_mdio_write_ge_ext(phy, 25U, register_value & 0xfffeU);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(phy, 31U, 2U);
    __const_udelay(429500UL);
    register_value = zx_mdio_read_ge_ext(phy, 17U);
    zx_mdio_write_ge_ext(phy, 17U, register_value & 0xc3ffU);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(phy, 31U, 0U);
    __const_udelay(429500UL);
    register_value = zx_mdio_read_ge_ext(phy, 29U);
    zx_mdio_write_ge_ext(phy, 29U, register_value & 0xfff0U);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(phy, 18U, 8U);
    zx_mdio_write_ge_ext(phy, 31U, 3U);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(phy, 16U, 0x01a0U);
    __const_udelay(429500UL);
    zx_mdio_write_ge_ext(phy, 31U, 0U);
    __const_udelay(429500UL);
    return printk("phy init: phy_id = 0x%x\n", phy);
}

int zte_gephy_set_eee_en(uint8_t phy, uint8_t enable)
{
    uint16_t register_value;

    zx_mdio_write(phy, 16U, 0xffff8001U);
    register_value = zx_mdio_read(phy, 17U) & 0xfff9U;
    if (enable == 1U)
        register_value |= 6U;
    zx_mdio_write(phy, 17U, register_value);
    return 0;
}

int zte_gephy_set_energy_detect_power_down_en(uint8_t phy, uint8_t enable)
{
    uint16_t register_value;

    register_value = zx_mdio_read(phy, 21U);
    zx_mdio_write(phy, 21U, (register_value & 0xfff7U) |
                                ((uint32_t)enable << 3));
    return 0;
}

int zte_gephy_set_link_status_change_en(uint8_t phy, uint16_t enable)
{
    uint16_t register_value;

    register_value = zx_mdio_read(phy, 24U);
    zx_mdio_write(phy, 24U, (register_value & 0xfffbU) |
                                ((uint32_t)(enable & 1U) << 2));
    return 0;
}

int zte_gephy_get_eee_en_status(uint8_t phy, uint8_t *status)
{
    if (status == 0) {
        printk("eee_en_status is null\n");
        return -1;
    }

    zx_mdio_write(phy, 16U, 0xffff8001U);
    *status = (uint8_t)((zx_mdio_read(phy, 17U) >> 1) & 3U);
    return 0;
}

int zte_gephy_get_short_reach_en(uint8_t phy, uint8_t *enable)
{
    volatile uint32_t *apb_base;

    if (enable == 0) {
        printk("eee_en_status is null\n");
        return -1;
    }

    apb_base = (volatile uint32_t *)(uintptr_t)sg_zxicgephy_apb_base[phy];
    if (apb_base != 0)
        *enable = (uint8_t)(apb_base[0x90U / sizeof(*apb_base)] & 1U);
    return 0;
}

int zte_gephy_get_energy_detect_power_down_en(uint8_t phy, uint8_t *enable)
{
    if (enable == 0) {
        printk("power_down_en is null\n");
        return -1;
    }

    (void)zx_mdio_read(phy, 21U);
    *enable = 0U;
    return 0;
}

int zte_gephy_get_1000m_tx_dac_lv(uint8_t phy, uint16_t *level)
{
    if (level == 0) {
        printk("tx_dac_lv is null\n");
        return -1;
    }

    zx_mdio_write(phy, 16U, 0xffffb407U);
    *level = zx_mdio_read(phy, 17U) & 0x003fU;
    return 0;
}

int zte_gephy_get_1000m_tx_dac_slew(uint8_t phy, uint16_t *slew)
{
    if (slew == 0) {
        printk("tx_dac_slew is null\n");
        return -1;
    }

    zx_mdio_write(phy, 16U, 0xffffb409U);
    *slew = zx_mdio_read(phy, 17U) & 7U;
    return 0;
}

int zte_gephy_get_100m_tx_dac_lv(uint8_t phy, uint16_t *level)
{
    if (level == 0) {
        printk("tx_dac_lv is null\n");
        return -1;
    }

    zx_mdio_write(phy, 16U, 0xffffb406U);
    *level = zx_mdio_read(phy, 17U) & 0x003fU;
    return 0;
}

int zte_gephy_get_100m_tx_dac_slew(uint8_t phy, uint16_t *slew)
{
    if (slew == 0) {
        printk("tx_dac_slew is null\n");
        return -1;
    }

    zx_mdio_write(phy, 16U, 0xffffb408U);
    *slew = zx_mdio_read(phy, 17U) & 7U;
    return 0;
}

int zte_gephy_get_link_status_change_en(uint8_t phy, uint16_t *enable)
{
    if (enable == 0) {
        printk("en is null\n");
        return -1;
    }

    *enable = (zx_mdio_read(phy, 24U) >> 2) & 1U;
    return 0;
}

int zte_gephy_get_link_status_change_event(uint8_t phy, uint16_t *occurred)
{
    if (occurred == 0) {
        printk("is_occurred is null\n");
        return -1;
    }

    *occurred = (zx_mdio_read(phy, 25U) >> 2) & 1U;
    return 0;
}

int zte_gephy_get_rx_stats(uint8_t phy)
{
    uint16_t counter;

    counter = zx_mdio_read(phy, 20U);
    printk("rx crc err cnt   : %d\n", counter);
    zx_mdio_write(phy, 16U, 0xffff9409U);
    counter = zx_mdio_read(phy, 17U);
    printk("rx cnt(H16)      : %d\n", counter);
    zx_mdio_write(phy, 16U, 0xffff940aU);
    counter = zx_mdio_read(phy, 17U);
    return printk("rx cnt(L16)      : %d\n", counter);
}

int zte_gephy_set_short_reach_en(uint8_t phy, uint8_t enable)
{
    uintptr_t apb_base;

    apb_base = sg_zxicgephy_apb_base[phy];
    if (apb_base != 0U)
        apb_bit_write((volatile uint32_t *)(apb_base + 0x90U), enable, 1U, 0U);
    return 0;
}

volatile uint32_t *zte_gephy_set_ref_clk_25M(uint8_t enable)
{
    return apb_bit_write(
        (volatile uint32_t *)(gephy_apb_base + 0x200018U),
        enable & 1U, 1U, 12U);
}

int zte_gephy_set_100m_tx_dac_lv(uint8_t phy, uint16_t level)
{
    uint16_t register_value;

    if (level > 0x003fU) {
        printk("tx_dac_lv is error. max is %u\n", 63U);
        return -1;
    }

    zx_mdio_write(phy, 16U, 0xffffb406U);
    register_value = zx_mdio_read(phy, 17U);
    zx_mdio_write(phy, 17U, (register_value & 0xffc0U) | level);
    return 0;
}

int zte_gephy_set_1000m_tx_dac_slew(uint8_t phy, uint16_t slew)
{
    uint16_t register_value;

    if (slew > 7U) {
        printk("tx_dac_slew is error. max is %u\n", 7U);
        return -1;
    }

    zx_mdio_write(phy, 16U, 0xffffb409U);
    register_value = zx_mdio_read(phy, 17U);
    zx_mdio_write(phy, 17U, (register_value & 0xfff8U) | slew);
    return 0;
}

int zte_gephy_set_100m_tx_dac_slew(uint8_t phy, uint16_t slew)
{
    uint16_t register_value;

    if (slew > 7U) {
        printk("tx_dac_slew is error. max is %u\n", 7U);
        return -1;
    }

    zx_mdio_write(phy, 16U, 0xffffb408U);
    register_value = zx_mdio_read(phy, 17U);
    zx_mdio_write(phy, 17U, (register_value & 0xfff8U) | slew);
    return 0;
}

int zte_gephy_set_1000m_tx_dac_lv(uint8_t phy, uint16_t level)
{
    uint16_t register_value;

    if (level > 0x003fU) {
        printk("tx_dac_lv is error. max is %u\n", 63U);
        return -1;
    }

    zx_mdio_write(phy, 16U, 0xffffb407U);
    register_value = zx_mdio_read(phy, 17U);
    zx_mdio_write(phy, 17U, (register_value & 0xffc0U) | level);
    return 0;
}

int check_phy_gephy(uint8_t phy)
{
    uint16_t saved_register_30;
    uint16_t status_register;
    unsigned int status_code;

    saved_register_30 = zx_mdio_read(phy, 30U);
    zx_mdio_write(phy, 30U, 0U);
    (void)zx_mdio_read(phy, 26U);
    __const_udelay(429500UL);
    status_register = zx_mdio_read(phy, 26U);
    zx_mdio_write(phy, 30U, saved_register_30);
    if ((status_register & 0x40U) == 0U)
        return -1;

    switch ((status_register >> 8) & 3U) {
    case 0U:
        status_code = 1U;
        break;
    case 1U:
        status_code = 2U;
        break;
    case 2U:
        status_code = 3U;
        break;
    default:
        status_code = 7U;
        break;
    }

    return (int)(status_code | (((status_register >> 7) & 1U) << 10));
}

int phy_zxicge_init(uint8_t unused_phy)
{
    (void)unused_phy;

    if (phy_zxicge_has_initialized != 0U)
        return phy_zxicge_has_initialized;

    sg_zxicgephy_apb_base[3] = gephy_apb_base;
    sg_zxicgephy_apb_base[0] = gephy_apb_base + 0x400000U;
    sg_zxicgephy_apb_base[1] = gephy_apb_base + 0x300000U;
    sg_zxicgephy_apb_base[2] = gephy_apb_base + 0x100000U;
    printk("init gephy apb base\n");
    phy_zxicge_has_initialized = 1U;
    return 1;
}

int zte_set_gephy_enable(uint8_t phy, uint8_t enable)
{
    uint16_t control_register;

    control_register = zx_mdio_read(phy, 0U);
    if (enable == 1U)
        control_register &= 0xf7ffU;
    else
        control_register |= 0x0800U;
    zx_mdio_write(phy, 0U, control_register);
    return 0;
}

int zte_get_gephy_enable(uintptr_t phy, uint8_t *enable)
{
    *enable = (zx_mdio_read((uint8_t)phy, 0U) & 0x0800U) == 0U;
    return 0;
}

int phy_zxic051_get_linkstate(uint8_t phy, uint8_t *link, uint8_t *duplex,
                               uint8_t *speed)
{
    *link = (uint8_t)outerphy_link[phy];
    *speed = (uint8_t)outerphy_link[phy + 4U];
    *duplex = (uint8_t)outerphy_link[phy + 8U];
    phy_zxic_speed_outer2uni(speed);
    return 0;
}

int phy_zxic051_set_enable(uint8_t phy, uint8_t enable)
{
    outerphy_link[phy + 12U] = enable;
    phy_zxic_051_set_enable();
    return 0;
}

int phy_zxic051_get_enable(void)
{
    return phy_zxic_051_get_enable();
}

int phy_zxic051_set_linkmode(uint8_t phy, uint8_t force_mode,
                              uint8_t duplex, uint8_t speed)
{
    uint8_t phy_with_offset;
    uint8_t phy_id;
    int32_t mdio_slot;
    uint32_t initial_value;
    uint32_t ge_write_value;
    uint32_t mode_value;

    phy_with_offset = (uint8_t)(phy + 4U);
    phy_id = phy_051_g_phy_id_check(phy_with_offset);
    if (phy_id == 0xffU) {
        if (__printk_ratelimit("phy_zxic051_set_linkmode") != 0)
            printk("051 phyid get err!\n");
        return -1;
    }

    mdio_slot = (int32_t)phy_with_offset - 4;
    if (force_mode == 1U) {
        initial_value = speed == 3U ? 0x2001U : 0x2081U;
        ge_write_value = 0x300U;
        mode_value = 0x1de1U;
    } else {
        switch (speed) {
        case 4U:
            initial_value = 0x2081U;
            ge_write_value = 0x300U;
            mode_value = 0x1de1U;
            break;
        case 3U:
            initial_value = 0x2001U;
            ge_write_value = 0x300U;
            mode_value = 0x1de1U;
            break;
        case 2U:
            initial_value = 0x2001U;
            ge_write_value = 0U;
            if (duplex == 1U)
                mode_value = 0x1de1U;
            else if (duplex == 0U)
                mode_value = 0x1ce1U;
            else
                return 0;
            break;
        case 1U:
            initial_value = 0x2001U;
            ge_write_value = 0U;
            if (duplex == 1U)
                mode_value = 0x1c61U;
            else if (duplex == 0U)
                mode_value = 0x1c21U;
            else
                return 0;
            break;
        default:
            return 0;
        }
    }

    zx_mdio_write_extended[mdio_slot](phy_id, 7U, 0x20U, initial_value);
    zx_mdio_write_ge_ext_by_port[mdio_slot](phy_id, 9U, ge_write_value);
    zx_mdio_write_extended[mdio_slot](phy_id, 7U, 0x10U, mode_value);
    zx_mdio_write_extended[mdio_slot](phy_id, 7U, 0U, 0x3200U);
    return 0;
}

int phy_zxic051_set_loopback(uint8_t phy, uint8_t enable)
{
    uint8_t phy_with_offset;
    uint8_t phy_id;
    int32_t mdio_slot;
    uint16_t register_value;

    phy_with_offset = (uint8_t)(phy + 4U);
    phy_id = phy_051_g_phy_id_check(phy_with_offset);
    if (phy_id == 0xffU) {
        if (__printk_ratelimit("phy_zxic051_set_loopback") != 0)
            printk("051 phyid get err!\n");
        return -1;
    }

    mdio_slot = (int32_t)phy_with_offset - 4;
    register_value = zx_mdio_read_ge_ext_by_port[mdio_slot](phy_id, 0U);
    if (enable == 1U) {
        zx_mdio_write_ge_ext_by_port[mdio_slot](phy_id, 0U,
                                                register_value | 0x4900U);
        register_value = zx_mdio_read_extended[mdio_slot](phy_id, 7U, 0U);
        zx_mdio_write_extended[mdio_slot](phy_id, 7U, 0U,
                                           register_value & 0xefffU);
        register_value = zx_mdio_read_extended[mdio_slot](phy_id, 1U, 0U);
        zx_mdio_write_extended[mdio_slot](phy_id, 1U, 0U,
                                           (register_value & 0xdfbfU) | 0x40U);
        register_value = zx_mdio_read_ge_ext_by_port[mdio_slot](phy_id, 0U);
        zx_mdio_write_ge_ext_by_port[mdio_slot](phy_id, 0U,
                                                register_value & 0xf7ffU);
        __const_udelay(4295000UL);
    } else {
        zx_mdio_write_ge_ext_by_port[mdio_slot](phy_id, 0U,
                                                (register_value & 0xb6ffU) | 0x800U);
        __const_udelay(4295000UL);
        register_value = zx_mdio_read_extended[mdio_slot](phy_id, 7U, 0U);
        zx_mdio_write_extended[mdio_slot](phy_id, 7U, 0U,
                                           register_value | 0x1000U);
        register_value = zx_mdio_read_extended[mdio_slot](phy_id, 1U, 0U);
        zx_mdio_write_extended[mdio_slot](phy_id, 1U, 0U,
                                           register_value | 0x2040U);
        __const_udelay(4295000UL);
        register_value = zx_mdio_read_ge_ext_by_port[mdio_slot](phy_id, 0U);
        zx_mdio_write_ge_ext_by_port[mdio_slot](phy_id, 0U,
                                                register_value & 0xf7ffU);
    }

    return 0;
}

int phy_zxic051_get_loopback(uint8_t phy, uint8_t *enabled)
{
    uint8_t phy_with_offset;
    uint8_t phy_id;
    int32_t mdio_slot;

    phy_with_offset = (uint8_t)(phy + 4U);
    phy_id = phy_051_g_phy_id_check(phy_with_offset);
    if (phy_id == 0xffU) {
        if (__printk_ratelimit("phy_zxic051_get_loopback") != 0)
            printk("051 phyid get err!\n");
        return -1;
    }

    mdio_slot = (int32_t)phy_with_offset - 4;
    *enabled = (uint8_t)((zx_mdio_read_ge_ext_by_port[mdio_slot](phy_id, 0U) >> 14) & 1U);
    return 0;
}

int phy_zxic051_get_linkmode(void)
{
    return phy_zxic_051_get_linkmode();
}

int phy_zxic051_init_check(uint8_t phy)
{
    uint8_t phy_id;
    int32_t mdio_slot;
    uint16_t selector_41_value;
    uint16_t selector_28_value;

    mdio_slot = (int32_t)phy - 4;
    phy_id = phy_051_g_phy_id_check(phy);
    selector_41_value = zx_mdio_read_extended[mdio_slot](phy_id, 31U, 41U);
    selector_28_value = zx_mdio_read_extended[mdio_slot](phy_id, 31U, 28U);
    if (selector_41_value == 0U && selector_28_value == 1U) {
        phy_zxic_051_phy_init(phy);
        return -1;
    }

    return 0;
}

int phy_zxic051_para_init(uint8_t phy)
{
    (void)phy_051_g_phy_id_check(phy);
    return phy_zxic051_init_check(phy);
}

unsigned int phy_zxic051_port_exist(uint8_t phy)
{
    return phy == 5U && is_certain_port_used(phy) != 0;
}

void pon_driver_unregister(void)
{
    platform_driver_unregister(zx_pon_driver);
}

void plat_cleanupModule(void)
{
    nppt_exit();
    pon_driver_unregister();
}

int dg_timer_init(void)
{
    init_timer_key(&dg_timer, dg_timer_func, 0U, 0U, 0U);
    dg_timer.expires = jiffies + 500UL;
    return add_timer(&dg_timer);
}

void dg_timer_func(void)
{
    uint32_t work_mode;

    hw_power_optx_set(1U);
    work_mode = g_pon_work_mode;
    if ((work_mode & 0xa0U) != 0U)
        *(volatile uint32_t *)(pon_base + 0x180000U) |= 1U;
    if ((work_mode & 0x100U) != 0U)
        *(volatile uint32_t *)(pon_base + 0x1c0004U) |= 1U;
    if ((work_mode & 0x40U) != 0U)
        *(volatile uint32_t *)(pon_base + 0x84000U) |= 9U;
    if ((work_mode & 0x600U) != 0U)
        *(volatile uint32_t *)(pon_base + 0x58400U) |= 1U;
    dg_flag = 0U;
}

void epon_set_dg_cnt(void)
{
    volatile uint32_t *counter_register;
    uint32_t counter_value;
    uint32_t work_mode;

    work_mode = g_pon_work_mode;
    if ((work_mode & 0xa0U) != 0U) {
        counter_register = (volatile uint32_t *)(pon_base + 0x1800f0U);
        counter_value = *counter_register;
        *counter_register = (counter_value & 0xfffffff0U) |
                            ((counter_value & 0xfU) << 1);
    }
    if ((work_mode & 0x100U) != 0U) {
        counter_register = (volatile uint32_t *)(pon_base + 0x1c0110U);
        counter_value = *counter_register;
        *counter_register = (counter_value & 0xfffffff0U) |
                            ((counter_value & 0xfU) << 1);
    }
}

void zxic_gpio_set_value(void)
{
}

unsigned int epon_get_llid_state(void)
{
    return (*(volatile uint32_t *)(pon_base + 0x180004U) >> 8) & 0xffU;
}

unsigned int xepon_get_llid_state(void)
{
    return (*(volatile uint32_t *)(pon_base + 0x1c0008U) >> 8) & 0xffU;
}

unsigned int xgpon_get_onu_state(void)
{
    return *(volatile uint32_t *)(pon_base + 0x59400U) & 7U;
}

unsigned int gpon_get_onu_state(void)
{
    return *(volatile uint32_t *)(pon_base + 0x94000U) & 7U;
}

unsigned int pon_is_registered(void)
{
    uint32_t work_mode;

    if (pon_registered != 0U)
        return 1U;

    work_mode = g_pon_work_mode;
    if ((work_mode & 0x600U) != 0U)
        pon_registered = xgpon_get_onu_state() == 5U;
    if ((work_mode & 0x40U) != 0U)
        pon_registered = gpon_get_onu_state() == 5U;
    if ((work_mode & 0xa0U) != 0U)
        pon_registered = epon_get_llid_state() != 0U;
    if ((work_mode & 0x100U) != 0U)
        pon_registered = xepon_get_llid_state() != 0U;
    return pon_registered;
}

void unregister_pon_int(void)
{
    free_irq(g_pon_irq, &pon_int_info);
}

void unregister_nppt_int(void)
{
    free_irq(g_nppt_irq, &pon_int_info);
}

volatile uint32_t *apb_write(volatile uint32_t *address, uint32_t value)
{
    *address = value;
    return address;
}

uint32_t apb_read(const volatile uint32_t *address)
{
    return *address;
}

volatile uint32_t *apb_bit_write(volatile uint32_t *address, uint32_t value,
                                 unsigned int width,
                                 unsigned int bit_offset)
{
    uint32_t field_mask;

    field_mask = ((1U << width) - 1U) << bit_offset;
    *address = (*address & ~field_mask) | (value << bit_offset);
    return address;
}

uint32_t an1_pll_en_cfg(uint32_t value)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = (volatile uint32_t *)(pon_serdes_pll_base + 0x10U);
    control_value = (*control_register & 0xfffffffeU) | value;
    *control_register = control_value;
    return control_value;
}

int serdes_err_cnt_reset(void)
{
    volatile uint32_t *control_register;

    control_register = (volatile uint32_t *)(pon_serdes_base + 0x94U);
    *control_register &= 0xffff7fffU;
    *control_register |= 0x8000U;
    return 0;
}

void serdes_unlock(void)
{
    *(volatile uint32_t *)(pon_serdes_base + 0x90U) &= 0xffff9fffU;
    *(volatile uint32_t *)(pon_serdes_base + 0x40U) &= 0xffff7fffU;
}

uint32_t an1_pll_en_get(void)
{
    uint32_t enabled;

    enabled = *(volatile uint32_t *)(pon_serdes_pll_base + 0x10U) & 1U;
    printk("an1 pll en is 0x%x\n", enabled);
    return enabled;
}

void an1_pll_bypass_cfg(uint32_t bypass_mode)
{
    volatile uint32_t *pll_reg_c;
    volatile uint32_t *pll_reg_10;
    volatile uint32_t *serdes_reg_c;
    volatile uint32_t *serdes_reg_10;
    volatile uint32_t *serdes_reg_54;
    volatile uint32_t *serdes_reg_64;
    volatile uint32_t *serdes_reg_74;

    if (bypass_mode > 2U)
        printk("bypass_mode is bigger than 2\n");

    pll_reg_c = (volatile uint32_t *)(pon_serdes_pll_base + 0xcU);
    pll_reg_10 = (volatile uint32_t *)(pon_serdes_pll_base + 0x10U);
    serdes_reg_c = (volatile uint32_t *)(pon_serdes_base + 0xcU);
    serdes_reg_10 = (volatile uint32_t *)(pon_serdes_base + 0x10U);
    serdes_reg_54 = (volatile uint32_t *)(pon_serdes_base + 0x54U);
    serdes_reg_64 = (volatile uint32_t *)(pon_serdes_base + 0x64U);
    serdes_reg_74 = (volatile uint32_t *)(pon_serdes_base + 0x74U);

    an1_pll_reg_c_snapshot = *pll_reg_c;
    an1_pll_reg_10_snapshot = *pll_reg_10;
    serdes_reg_c_snapshot = *serdes_reg_c;
    serdes_reg_10_snapshot = *serdes_reg_10;
    serdes_reg_54_snapshot = *serdes_reg_54;
    serdes_reg_64_snapshot = *serdes_reg_64;
    serdes_reg_74_snapshot = *serdes_reg_74;

    if (bypass_mode == 0U) {
        *pll_reg_c = an1_pll_reg_c_snapshot;
        *pll_reg_10 = an1_pll_reg_10_snapshot;
        *serdes_reg_c = serdes_reg_c_snapshot;
        *serdes_reg_10 = serdes_reg_10_snapshot;
        *serdes_reg_54 = serdes_reg_54_snapshot;
        *serdes_reg_64 = serdes_reg_64_snapshot;
        *serdes_reg_74 = serdes_reg_74_snapshot;
        return;
    }

    if (bypass_mode != 1U && bypass_mode != 2U) {
        printk("bypass_mode is error \n");
        return;
    }

    *pll_reg_c |= 0x80U;
    *pll_reg_10 &= 0xfffffffeU;
    if (bypass_mode == 1U) {
        *serdes_reg_c &= 0xfff7ffffU;
        *serdes_reg_c &= 0xffefffffU;
        *serdes_reg_c = (*serdes_reg_c & 0x003fffffU) | 0x19000000U;
    } else {
        *serdes_reg_c |= 0x00080000U;
        *serdes_reg_c |= 0x00100000U;
        *serdes_reg_c = (*serdes_reg_c & 0x003fffffU) | 0x18c00000U;
    }
    *serdes_reg_54 |= 8U;
    *serdes_reg_64 |= 1U;
    *serdes_reg_64 |= 2U;
    *serdes_reg_64 &= 0xffffff7fU;
    *serdes_reg_64 = (*serdes_reg_64 & 0xffffcfffU) | 0x2000U;
    *serdes_reg_74 = (*serdes_reg_74 & 0xffff9fffU) | 0x4000U;
}

uint32_t an1_pll_bypass_get(void)
{
    uint32_t bypass_enabled;

    bypass_enabled =
        (*(volatile uint32_t *)(pon_serdes_pll_base + 0xcU) >> 7) & 1U;
    printk("an1_pll_bypass_ is 0x%x\n", bypass_enabled);
    return bypass_enabled;
}

int an1_pll_out_mode_cfg(uint32_t enable)
{
    volatile uint32_t *control_register;

    control_register = (volatile uint32_t *)(pon_serdes_pll_base + 0x1cU);
    *control_register = (*control_register & 0xfffffffdU) | (enable << 1);
    return printk("en=1 is 156.25M   ,   en=0  is 155.52M\n");
}

uint32_t an1_pll_out_mode_get(void)
{
    uint32_t mode;

    mode = (*(volatile uint32_t *)(pon_serdes_pll_base + 0x1cU) >> 1) & 1U;
    if (mode != 0U)
        printk("an1_pll_out_ is 156.25M \n");
    else
        printk("an1_pll_out_ is 155.52M \n");
    return mode;
}

int an1_pll_cfg_ring_circle_bisa_set(uint32_t value)
{
    volatile uint32_t *control_register;

    control_register = (volatile uint32_t *)(pon_serdes_pll_base + 4U);
    *control_register = (*control_register & 0xfff0ffffU) | (value << 16);
    return printk("AN1 pll ring circle I set data=0x%x\n", value);
}

uint32_t an1_pll_cfg_ring_circle_bisa_get(void)
{
    uint32_t value;

    value = (*(volatile uint32_t *)(pon_serdes_pll_base + 4U) >> 16) & 0xfU;
    printk("an1_pll_cfg_ring_circle_bisa is 0x%x\n", value);
    return value;
}

int an1_pll_cfg_ring_circle_resl_set(uint32_t value)
{
    volatile uint32_t *control_register;

    control_register = (volatile uint32_t *)(pon_serdes_pll_base + 4U);
    *control_register = (*control_register & 0xf87fffffU) | (value << 23);
    return printk("AN1 pll ring circle R set data=0x%x\n", value);
}

uint32_t an1_pll_cfg_ring_circle_resl_get(void)
{
    uint32_t value;

    value = (*(volatile uint32_t *)(pon_serdes_pll_base + 4U) >> 23) & 0xfU;
    printk("an1_pll_cfg_ring_circle_resl is 0x%x\n", value);
    return value;
}

int com_pll_cfg_ring_circle_bisa_set(uint32_t value)
{
    volatile uint32_t *control_register;

    control_register = (volatile uint32_t *)(pon_serdes_base + 4U);
    *control_register = (*control_register & 0xfff0ffffU) | (value << 16);
    printk("default:0100 100U           0000:0 0001:25u 0010:50u 0011:75u "
           "0100:100u 0101:125u 0110:150u 0111:175u           1000:200u "
           "1001:225u 1010:250u 1011:275u 1100:300u 1101:325u 1110:350u "
           "1111:375u \n");
    return printk("com pll ring circle I set data=0x%x\n", value);
}

uint32_t com_pll_cfg_ring_circle_bisa_get(void)
{
    uint32_t value;

    value = (*(volatile uint32_t *)(pon_serdes_base + 4U) >> 16) & 0xfU;
    printk("an1_pll_cfg_ring_circle_bisa is 0x%x\n", value);
    printk("default:0100 100U           0000:0 0001:25u 0010:50u 0011:75u "
           "0100:100u 0101:125u 0110:150u 0111:175u           1000:200u "
           "1001:225u 1010:250u 1011:275u 1100:300u 1101:325u 1110:350u "
           "1111:375u \n");
    return value;
}

int com_pll_cfg_ring_circle_resl_set(uint32_t value)
{
    volatile uint32_t *control_register;

    control_register = (volatile uint32_t *)(pon_serdes_base + 4U);
    *control_register = (*control_register & 0xf87fffffU) | (value << 23);
    return printk("com pll ring circle R set data=0x%x\n", value);
}

uint32_t com_pll_cfg_ring_circle_resl_get(void)
{
    uint32_t value;

    value = (*(volatile uint32_t *)(pon_serdes_base + 4U) >> 23) & 0xfU;
    printk("an1_pll_cfg_ring_circle_resl is 0x%x\n", value);
    return value;
}

int serdes_set_tx_swin(uint32_t value)
{
    volatile uint32_t *control_register;

    control_register = (volatile uint32_t *)(pon_serdes_base + 0x20U);
    *control_register = (*control_register & 0xfffcffffU) | (value << 16);
    return printk("set the tx swin is 0x%x\n", value);
}

int serdes_set_low_power(uint32_t mode)
{
    volatile uint32_t *power_register;

    if (mode > 5U)
        return printk("LOW POWER MODE IS ERROR\n");

    power_register = (volatile uint32_t *)(pon_serdes_base + 0x5cU);
    switch (mode) {
    case 0U:
        *power_register &= 0xffffff00U;
        return printk("enter normal mode\n");
    case 1U:
        *power_register |= 0xffU;
        return printk("enter low power mode\n");
    case 2U:
        *power_register = (*power_register & 0xffffff00U) | 0xddU;
        return printk("enter sleep mode\n");
    case 3U:
        *power_register = (*power_register & 0xffffff00U) | 0x22U;
        return printk("enter small flow mode\n");
    case 4U:
        *power_register = (*power_register & 0xffffff00U) | 0x33U;
        return printk("enter rx en and tx off mode\n");
    default:
        return printk("the low power mode is error\n");
    }
}

int serdes_set_band(uint32_t band_select, uint32_t band)
{
    volatile uint32_t *band_register;

    band_register = (volatile uint32_t *)(pon_serdes_base + 0x6cU);
    *band_register = (*band_register & 0xffffbfffU) | (band_select << 14);
    *band_register = (*band_register & 0xffffff00U) | band;
    return printk("set pll band ok\n");
}

uint32_t serdes_get_band(void)
{
    uint32_t band;

    band = (*(volatile uint32_t *)(pon_serdes_base + 0xd0U) >> 16) & 0xffU;
    printk("serdes_get_band is 0x%x\n", band);
    return band;
}

int serdes_set_gen_en(uint32_t enable)
{
    volatile uint32_t *prbs_control_register;

    prbs_control_register = (volatile uint32_t *)(pon_serdes_base + 0x94U);
    *prbs_control_register = (*prbs_control_register & 0xffffdfffU) |
                             (enable << 13);
    return printk("set the prbs gen en=0x%x\n", enable);
}

int serdes_set_check_en(uint32_t enable)
{
    volatile uint32_t *prbs_control_register;

    prbs_control_register = (volatile uint32_t *)(pon_serdes_base + 0x94U);
    *prbs_control_register = (*prbs_control_register & 0xffffbfffU) |
                             (enable << 14);
    return printk("set check en =0x%x\n", enable);
}

int serdes_set_err_cnt_en(uint32_t enable)
{
    volatile uint32_t *prbs_control_register;

    prbs_control_register = (volatile uint32_t *)(pon_serdes_base + 0x94U);
    *prbs_control_register = (*prbs_control_register & 0xffff7fffU) |
                             (enable << 15);
    return printk("set prbs error cnt  en =0x%x\n", enable);
}

uint64_t serdes_get_err_cnt(void)
{
    uint64_t error_count;

    error_count = *(volatile uint32_t *)(pon_serdes_base + 0xe8U);
    error_count |= (uint64_t)(*(volatile uint32_t *)(pon_serdes_base + 0xecU) &
                              0xffffU) << 32;
    printk("serdes_get_err_cnt =0x%lx\n", error_count);
    return error_count;
}

int serdes_prbs_err_ok(void)
{
    *(volatile uint32_t *)(pon_serdes_base + 0x48U) |= 0x00800000U;
    return printk("set 1 bit error ok\n");
}

int serdes_set_error_time(uint32_t seconds)
{
    volatile uint32_t *time_low_register;
    volatile uint32_t *time_high_register;
    uint32_t time_low;
    uint64_t programmed_time;

    time_low = 0U;
    if (pon_serdes_mode <= 4U)
        time_low = seconds * 156250000U;
    if (pon_serdes_mode - 5U <= 2U)
        time_low = seconds * 155520000U;

    time_low_register = (volatile uint32_t *)(pon_serdes_base + 0x98U);
    time_high_register = (volatile uint32_t *)(pon_serdes_base + 0xa4U);
    *time_low_register = time_low;
    *time_high_register &= 0x00ffffffU;

    programmed_time = *time_low_register;
    programmed_time |= (uint64_t)((*time_high_register >> 24) & 0xffU) << 32;
    return printk("serdes_set_error_time : %ld.\n", programmed_time);
}

void serdesPrbsCounterGetHandler(void)
{
    uint64_t error_count;

    error_count = serdes_get_err_cnt();
    if (error_count < serdesPrbsCounter) {
        printk("pon serdes ztePonGetPrbsCounters Error: Overflow detected\n");
        return;
    }

    printk("pon serdes ztePonGetPrbsCounters counter: %ld.\n",
           error_count - serdesPrbsCounter);
}

int serdes_set_loopback_mode(uint32_t mode, int prbs_enable)
{
    volatile uint32_t *registers;
    const char *message;

    registers = (volatile uint32_t *)pon_serdes_base;
    if (serdes_loopback_call_count != 0U) {
        registers[0x1cU / 4U] = serdes_loopback_reg_1c_snapshot;
        registers[0x24U / 4U] = serdes_loopback_reg_24_snapshot;
        registers[0x40U / 4U] = serdes_loopback_reg_40_snapshot;
        registers[0x48U / 4U] = serdes_loopback_reg_48_snapshot;
        registers[0x90U / 4U] = serdes_loopback_reg_90_snapshot;
        registers[0x94U / 4U] = serdes_loopback_reg_94_snapshot;
        printk("pon from the second time , should recovery the default data\n");
    } else {
        serdes_loopback_reg_1c_snapshot = registers[0x1cU / 4U];
        serdes_loopback_reg_24_snapshot = registers[0x24U / 4U];
        serdes_loopback_reg_40_snapshot = registers[0x40U / 4U];
        serdes_loopback_reg_48_snapshot = registers[0x48U / 4U];
        serdes_loopback_reg_90_snapshot = registers[0x90U / 4U];
        serdes_loopback_reg_94_snapshot = registers[0x94U / 4U];
        printk("pon first time remmber default data\n");
    }

    if (mode > 10U)
        return printk("the loop mode is big than PATH_MODE\n");

    switch (mode) {
    case 0U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x20000U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] =
            (registers[0x40U / 4U] & 0xfffe7fffU) | 0x8000U;
        registers[0x48U / 4U] &= 0xffff8fffU;
        registers[0x48U / 4U] &= ~0x20000U;
        registers[0x48U / 4U] |= 0x40000U;
        registers[0x48U / 4U] &= ~0x200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x4000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 1U;
        if (prbs_enable == 1) {
            registers[0x94U / 4U] |= 0xe000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        message = "enter loop back PATH1_TX2RX_PCS_LOOP0 \n";
        break;
    case 1U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x20000U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] =
            (registers[0x40U / 4U] & 0xfffe7fffU) | 0x8000U;
        registers[0x48U / 4U] &= 0xffff8fffU;
        registers[0x48U / 4U] &= ~0x20000U;
        registers[0x48U / 4U] |= 0x40000U;
        registers[0x48U / 4U] &= ~0x200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x4000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 2U;
        if (prbs_enable == 1) {
            registers[0x94U / 4U] |= 0xe000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        message = "enter loop back PATH1_TX2RX_PCS_LOOP1 \n";
        break;
    case 2U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x20000U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] =
            (registers[0x40U / 4U] & 0xfffe7fffU) | 0x8000U;
        registers[0x48U / 4U] &= 0xffff8fffU;
        registers[0x48U / 4U] &= ~0x20000U;
        registers[0x48U / 4U] |= 0x40000U;
        registers[0x48U / 4U] &= ~0x200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x4000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 3U;
        if (prbs_enable == 1) {
            registers[0x94U / 4U] |= 0xe000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        message = "enter loop back PATH1_TX2RX_PCS_LOOP2 \n";
        break;
    case 3U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x20000U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] &= 0xfffe7fffU;
        registers[0x48U / 4U] &= 0xffff8fffU;
        registers[0x48U / 4U] &= ~0x20000U;
        registers[0x48U / 4U] |= 0x40000U;
        registers[0x48U / 4U] &= ~0x200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x4000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 4U;
        if (prbs_enable == 1) {
            registers[0x94U / 4U] |= 0xe000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        message = "enter loop back PATH2_TX2RX_CABLE_LOOP \n";
        break;
    case 4U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x20000U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xffe7ffffU) | 0x80000U;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] =
            (registers[0x40U / 4U] & 0xfffe7fffU) | 0x8000U;
        registers[0x48U / 4U] &= 0xffff8fffU;
        registers[0x48U / 4U] |= 0x20000U;
        registers[0x48U / 4U] |= 0x40000U;
        registers[0x48U / 4U] &= ~0x200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x4000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 5U;
        if (prbs_enable == 1) {
            registers[0x94U / 4U] |= 0xe000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        message = "enter loop back PATH3_TX2RX_PMA_LOOP \n";
        break;
    case 5U:
        registers[0x1cU / 4U] |= 0x100U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] &= 0xfffe7fffU;
        registers[0x48U / 4U] &= ~0x20000U;
        registers[0x48U / 4U] &= ~0x40000U;
        registers[0x48U / 4U] &= ~0x200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x4000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 6U;
        if (prbs_enable == 1) {
            registers[0x94U / 4U] |= 0xe000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        message = "enter loop back PATH4_RX2TX_PCS_LOOP \n";
        break;
    case 6U:
        registers[0x1cU / 4U] |= 0x100U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xff9fffffU) | 0x200000U;
        registers[0x40U / 4U] &= 0xfffe7fffU;
        registers[0x48U / 4U] &= ~0x20000U;
        registers[0x48U / 4U] &= ~0x40000U;
        registers[0x48U / 4U] |= 0x200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x4000U;
        registers[0x94U / 4U] &= 0xfffffff8U;
        if (prbs_enable == 1) {
            registers[0x94U / 4U] |= 0xe000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        message = "enter loop back PATH5_RX2TX_PMA_LOOP \n";
        break;
    case 7U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x20000U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] &= 0xfffe7fffU;
        registers[0x48U / 4U] =
            (registers[0x48U / 4U] & 0xffff8fffU) | 0x2000U;
        registers[0x48U / 4U] &= ~0x20000U;
        registers[0x48U / 4U] &= ~0x40000U;
        registers[0x48U / 4U] &= ~0x200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x4000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 1U;
        if (prbs_enable == 1) {
            registers[0x94U / 4U] |= 0xe000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        message = "enter loop back PATH6_RX_RECEIVE \n";
        break;
    case 8U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x20000U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] &= 0xfffe7fffU;
        registers[0x48U / 4U] =
            (registers[0x48U / 4U] & 0xffff8fffU) | 0x2000U;
        registers[0x48U / 4U] &= ~0x20000U;
        registers[0x48U / 4U] &= ~0x40000U;
        registers[0x48U / 4U] &= ~0x200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x4000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 1U;
        if (prbs_enable == 1) {
            registers[0x94U / 4U] |= 0xe000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        message = "enter loop back PATH6_TRANSMIT \n";
        break;
    case 9U:
        message = "recovery the default data\n";
        break;
    default:
        message = "the path mode is error\n";
        break;
    }

    printk(message);
    return (int)++serdes_loopback_call_count;
}

int serdes_set_rx_eq_mbf(uint32_t value)
{
    volatile uint32_t *equalizer_register;

    equalizer_register = (volatile uint32_t *)(pon_serdes_base + 0x2cU);
    *equalizer_register = (*equalizer_register & 0xffc3ffffU) | (value << 18);
    return printk("set serdes rx eq mbf 0x%x\n", value);
}

int serdes_get_rx_eq(void)
{
    uint32_t equalizer;

    equalizer = *(volatile uint32_t *)(pon_serdes_base + 0x2cU);
    if ((equalizer & 1U) == 0U) {
        printk("serdes rx eq1 is enable");
        printk("serdes rx eq1 data is 0x%x\n", (equalizer >> 3) & 0x1fU);
    } else {
        printk("serdes rx eq1 is disable");
    }

    if ((equalizer & 2U) == 0U) {
        printk("serdes rx eq2 is enable");
        printk("serdes rx eq2 data is 0x%x\n", (equalizer >> 8) & 0x1fU);
    } else {
        printk("serdes rx eq2 is disable");
    }

    if ((equalizer & 4U) == 0U) {
        printk("serdes rx eq3 is enable");
        printk("serdes rx eq3 data is 0x%x\n", (equalizer >> 13) & 0x1fU);
    } else {
        printk("serdes rx eq3 is disable");
    }

    return printk("serdes rx eq mbf is 0x%x\n",
                  (equalizer >> 18) & 0xfU);
}

int serdes_set_np_jittery(uint32_t value)
{
    volatile uint32_t *jitter_register;

    jitter_register = (volatile uint32_t *)(pon_serdes_base + 0x48U);
    *jitter_register = (*jitter_register & 0xfffffe3fU) | (value << 6);
    return printk("serdes_np_jittery_set 0x%x\n", value);
}

uint32_t serdes_get_np_jittery(void)
{
    uint32_t value;

    value = (*(volatile uint32_t *)(pon_serdes_base + 0x48U) >> 6) & 7U;
    printk("serdes_np_jittery_get 0x%x\n", value);
    return value;
}

int check_serdes_version(void)
{
    uint32_t version_selector;
    uint32_t legacy_signature;

    version_selector =
        (*(volatile uint32_t *)(pon_serdes_base + 4U) >> 1) & 0xfU;
    legacy_signature =
        *(volatile uint32_t *)(pon_serdes_base + 0x18U) >> 16;

    if (version_selector == 1U)
        return printk("pon serdes is V1 version\n");
    if (version_selector != 0U)
        return printk("pon serdes version is error \n");
    if (legacy_signature == 0xef0U)
        return printk("pon serdes is V2 version\n");
    if (legacy_signature == 0xffU)
        return printk("pon serdes is V3 version\n");
    return printk("pon serdes version is error \n");
}

void check_serdes_config(void)
{
    uint32_t offset;

    switch (pon_serdes_mode) {
    case 0U:
        printk("MODE_EPON CONFIG \n");
        break;
    case 1U:
        printk("MODE_10G_EPON_NSYN_DPLL CONFIG \n");
        break;
    case 2U:
        printk("MODE_10G_EPON_NSYN_FIFO CONFIG \n");
        break;
    case 3U:
        printk("MODE_10G_EPON_NSYN_NO_FIFO CONFIG \n");
        break;
    case 4U:
        printk("MODE_10G_EPON_SYN CONFIG \n");
        break;
    case 5U:
        printk("MODE_GPON CONFIG \n");
        break;
    case 6U:
        printk("MODE_XGPON_NSYN CONFIG \n");
        break;
    case 7U:
        printk("MODE_XGPON_SYN CONFIG \n");
        break;
    case 8U:
        printk("MODE_ETH_SGMII CONFIG \n");
        break;
    case 9U:
        printk("MODE_ETH_HSGMII CONFIG \n");
        break;
    case 10U:
        printk("MODE_ETH_USXGMII_2P5G CONFIG \n");
        break;
    case 12U:
        printk("MODE_ETH_USXGMII_5G CONFIG \n");
        break;
    case 13U:
        printk("MODE_ETH_10GBASE_R CONFIG \n");
        break;
    case 14U:
        printk("MODE_ETH_USXGMII_10G CONFIG \n");
        break;
    case 15U:
        printk("MODE_ETH_2P5BASE_X CONFIG \n");
        break;
    case 16U:
        printk("MODE_ETH_1GBASE_X CONFIG \n");
        break;
    default:
        printk("ERROR MODE CONFIG\n");
        break;
    }

    for (offset = 0U; offset != 0x128U; offset += 4U) {
        printk("pon serdes  addr:0x%x,value:0x%x\n",
               0x16000000U + offset,
               *(volatile uint32_t *)(pon_serdes_base + offset));
    }

    printk("#########################################################\n");
    if (isCpuType_132() == 1U) {
        for (offset = 0U; offset != 0x28U; offset += 4U) {
            printk("serdes  addr:0x%x,value:0x%x\n",
                   0x16010000U + offset,
                   *(volatile uint32_t *)(pon_serdes_pll_base + offset));
        }
    }
}

int serdes_set_tx_prbs_mode(uint32_t mode)
{
    volatile uint32_t *mode_register;
    volatile uint32_t *prbs_control_register;

    mode_register = (volatile uint32_t *)(pon_serdes_base + 0x24U);
    prbs_control_register = (volatile uint32_t *)(pon_serdes_base + 0x94U);

    serdes_set_gen_en(1U);
    if (isCpuType_132() == 1U) {
        *mode_register &= 0xfff8ffffU;
        *prbs_control_register =
            (*prbs_control_register & 0xfffffff8U) | 4U;
    } else if (isCpuType_133() == 1U) {
        *mode_register = (*mode_register & 0xfff8ffffU) | 0x20000U;
        *prbs_control_register =
            (*prbs_control_register & 0xfffffff8U) | 4U;
        *prbs_control_register |= 0x80000000U;
    } else if (isCpuType_129() == 1U) {
        *mode_register = (*mode_register & 0xfff8ffffU) | 0x20000U;
        *prbs_control_register =
            (*prbs_control_register & 0xfffffcffU) | 0x200U;
        *prbs_control_register =
            (*prbs_control_register & 0xfffff3ffU) | 0x800U;
        *prbs_control_register =
            (*prbs_control_register & 0xfffffff8U) | 4U;
        *prbs_control_register |= 0x80000000U;
    }

    switch (mode) {
    case 0U:
        *prbs_control_register &= 0xfff8ffffU;
        printk("\nserdes_set_tx_prbs_prbs_mode to 7\n");
        break;
    case 1U:
        *(volatile uint32_t *)(pon_serdes_base + 0xa4U) = 0x00555555U;
        *prbs_control_register =
            (*prbs_control_register & 0xfff8ffffU) | 0x40000U;
        printk("\nserdes_set_tx_prbs_prbs_mode to 23\n");
        break;
    case 2U:
        *(volatile uint32_t *)(pon_serdes_base + 0xa4U) = 0x00555555U;
        *prbs_control_register =
            (*prbs_control_register & 0xfff8ffffU) | 0x50000U;
        printk("\nserdes_set_tx_prbs_prbs_mode to 31\n");
        break;
    case 3U:
        *prbs_control_register =
            (*prbs_control_register & 0xfff8ffffU) | 0x10000U;
        printk("\nserdes_set_tx_prbs_prbs_mode to 9\n");
        break;
    case 4U:
        *prbs_control_register =
            (*prbs_control_register & 0xfff8ffffU) | 0x30000U;
        printk("\nserdes_set_tx_prbs_prbs_mode to 15\n");
        break;
    case 5U:
        serdes_set_gen_en(0U);
        *(volatile uint32_t *)(pon_serdes_base + 0x9cU) = 0x55555500U;
        *(volatile uint32_t *)(pon_serdes_base + 0xa0U) = 0x55555555U;
        *(volatile uint32_t *)(pon_serdes_base + 0xa4U) = 0x07555555U;
        printk("\nserdes_set_tx_prbs_prbs_mode to 0101\n");
        break;
    default:
        break;
    }

    return 0;
}

int serdes_set_rx_prbs_mode(uint32_t mode)
{
    volatile uint32_t *rx_control_register;
    volatile uint32_t *prbs_control_register;

    rx_control_register = (volatile uint32_t *)(pon_serdes_base + 0x48U);
    prbs_control_register = (volatile uint32_t *)(pon_serdes_base + 0x94U);

    if (isCpuType_132() == 1U) {
        *rx_control_register &= 0xffff8fffU;
        *rx_control_register |= 0x10000U;
        *prbs_control_register =
            (*prbs_control_register & 0xfffffff8U) | 4U;
    } else if (isCpuType_133() == 1U) {
        *rx_control_register =
            (*rx_control_register & 0xffff8fffU) | 0x2000U;
        *prbs_control_register =
            (*prbs_control_register & 0xfffffff8U) | 4U;
        *prbs_control_register |= 0x80000000U;
    } else if (isCpuType_129() == 1U) {
        *rx_control_register =
            (*rx_control_register & 0xffff8fffU) | 0x2000U;
        *prbs_control_register =
            (*prbs_control_register & 0xfffffcffU) | 0x200U;
        *prbs_control_register =
            (*prbs_control_register & 0xfffff3ffU) | 0x800U;
        *prbs_control_register =
            (*prbs_control_register & 0xfffffff8U) | 4U;
        *prbs_control_register |= 0x80000000U;
    }

    switch (mode) {
    case 0U:
        *prbs_control_register &= 0xffc7ffffU;
        printk("\nserdes_set_rx_prbs_prbs_mode to 7\n");
        break;
    case 1U:
        *prbs_control_register =
            (*prbs_control_register & 0xffc7ffffU) | 0x200000U;
        printk("\nserdes_set_rx_prbs_prbs_mode to 23\n");
        break;
    case 2U:
        *prbs_control_register =
            (*prbs_control_register & 0xffc7ffffU) | 0x280000U;
        printk("\nserdes_set_rx_prbs_prbs_mode to 31\n");
        break;
    case 3U:
        *prbs_control_register =
            (*prbs_control_register & 0xffc7ffffU) | 0x80000U;
        printk("\nserdes_set_rx_prbs_prbs_mode to 9\n");
        break;
    case 4U:
        *prbs_control_register =
            (*prbs_control_register & 0xffc7ffffU) | 0x180000U;
        printk("\nserdes_set_rx_prbs_prbs_mode to 15\n");
        break;
    default:
        break;
    }

    return 0;
}

int serdes_set_sprbsrxbist(int prbs_mode, uint32_t rx_bist_enable)
{
    serdes_set_rx_prbs_mode((uint32_t)prbs_mode - 1U);
    serdes_set_check_en(rx_bist_enable);
    serdes_set_err_cnt_en(rx_bist_enable);
    return printk("%s PrbsMode=%d\n, RxBistEnable=%d\n",
                  "serdes_set_sprbsrxbist", prbs_mode, rx_bist_enable);
}

void serdes_set_pattern(uint32_t pattern_31_0, uint32_t pattern_63_32,
                        uint16_t pattern_79_64, int enable)
{
    volatile uint32_t *pattern_high_register;

    *(volatile uint32_t *)(pon_serdes_base + 0x94U) &= 0xffff0fffU;
    if (isCpuType_133() == 1U || isCpuType_129() == 1U)
        *(volatile uint32_t *)(pon_serdes_base + 0x24U) &= 0xfff8ffffU;

    *(volatile uint32_t *)(pon_serdes_base + 0x9cU) = pattern_31_0;
    *(volatile uint32_t *)(pon_serdes_base + 0xa0U) = pattern_63_32;
    pattern_high_register = (volatile uint32_t *)(pon_serdes_base + 0xa4U);
    *pattern_high_register = (*pattern_high_register & 0xffff0000U) |
                             pattern_79_64;
    if (enable == 1)
        *pattern_high_register |= 0x70000U;
    else
        *pattern_high_register &= 0xfff8ffffU;
}

void check_serdes_lock(void)
{
    uint32_t pll_status;
    uint32_t cdr_status;
    uint32_t alos_status;

    if (isCpuType_132() == 1U) {
        uint32_t an1_pll_status;

        an1_pll_status =
            *(volatile uint32_t *)(pon_serdes_pll_base + 0x20U) & 1U;
        pll_status = *(volatile uint32_t *)(pon_serdes_base + 0xd0U) & 1U;
        cdr_status =
            (*(volatile uint32_t *)(pon_serdes_base + 0xe4U) >> 1) & 1U;
        alos_status = *(volatile uint32_t *)(pon_serdes_base + 0xe4U) & 1U;
        printk("an1_pll_sta=0x%x pll_sta=0x%x cdr_sta=0x%x alos_data=0x%x\n",
               an1_pll_status, pll_status, cdr_status, alos_status);
    }

    if (isCpuType_133() == 1U) {
        pll_status = *(volatile uint32_t *)(pon_serdes_base + 0xd0U) & 1U;
        cdr_status =
            (*(volatile uint32_t *)(pon_serdes_base + 0xe4U) >> 1) & 1U;
        alos_status = *(volatile uint32_t *)(pon_serdes_base + 0xe4U) & 1U;
        printk(" pll_sta=0x%x cdr_sta=0x%x alos_data=0x%x\n",
               pll_status, cdr_status, alos_status);
    }

    if (isCpuType_129() == 1U) {
        pll_status =
            (*(volatile uint32_t *)(pon_serdes_base + 0xccU) >> 1) & 1U;
        cdr_status =
            (*(volatile uint32_t *)(pon_serdes_base + 0xe4U) >> 1) & 1U;
        alos_status = *(volatile uint32_t *)(pon_serdes_base + 0xe4U) & 1U;
        printk(" pll_sta=0x%x cdr_sta=0x%x alos_data=0x%x\n",
               pll_status, cdr_status, alos_status);
    }
}

int get_all_efuse(void)
{
    uint32_t word0;
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;
    uint32_t word4;
    uint32_t word5;
    uint32_t word6;
    uint32_t word7;

    if (isCpuType_129() == 1U) {
        printk("================   REG VAL   ================\n");
        printk("ZXIC_EFUSE_0_REG(0x14f11000)  = %#x", EFUSE_U32(0x0U));
        printk("ZXIC_EFUSE_1_REG(0x14f11004)  = %#x", EFUSE_U32(0x4U));
        printk("ZXIC_EFUSE_2_REG(0x14f11008)  = %#x", EFUSE_U32(0x8U));
        printk("ZXIC_EFUSE_3_REG(0x14f1100C)  = %#x", EFUSE_U32(0xcU));
        printk("ZXIC_EFUSE_4_REG(0x14f11010)  = %#x", EFUSE_U32(0x10U));
        printk("ZXIC_EFUSE_5_REG(0x14f11014)  = %#x", EFUSE_U32(0x14U));
        printk("ZXIC_EFUSE_6_REG(0x14f11018)  = %#x", EFUSE_U32(0x18U));
        printk("ZXIC_EFUSE_7_REG(0x14f1101C)  = %#x", EFUSE_U32(0x1cU));
        printk("ZXIC_EFUSE_8_REG(0x14f11020)  = %#x", EFUSE_U32(0x20U));
        printk("ZXIC_EFUSE_9_REG(0x14f11024)  = %#x", EFUSE_U32(0x24U));
        printk("ZXIC_EFUSE_10_REG(0x14f11028) = %#x", EFUSE_U32(0x28U));
        printk("ZXIC_EFUSE_11_REG(0x14f1102C) = %#x", EFUSE_U32(0x2cU));
        printk("ZXIC_EFUSE_12_REG(0x14f11030) = %#x", EFUSE_U32(0x30U));
        printk("ZXIC_EFUSE_13_REG(0x14f11034) = %#x", EFUSE_U32(0x34U));
        printk("ZXIC_EFUSE_14_REG(0x14f11038) = %#x", EFUSE_U32(0x38U));
        printk("ZXIC_EFUSE_15_REG(0x14f1103C) = %#x", EFUSE_U32(0x3cU));
        printk("ZXIC_EFUSE_16_REG(0x14f11040) = %#x", EFUSE_U32(0x40U));
        printk("ZXIC_EFUSE_17_REG(0x14f11044) = %#x", EFUSE_U32(0x44U));
        printk("ZXIC_EFUSE_18_REG(0x14f11048) = %#x", EFUSE_U32(0x48U));
        printk("ZXIC_EFUSE_19_REG(0x14f1104C) = %#x", EFUSE_U32(0x4cU));
        printk("ZXIC_EFUSE_20_REG(0x14f11050) = %#x", EFUSE_U32(0x50U));
        printk("ZXIC_EFUSE_21_REG(0x14f11054) = %#x", EFUSE_U32(0x54U));
        printk("ZXIC_EFUSE_22_REG(0x14f11058) = %#x", EFUSE_U32(0x58U));
        printk("ZXIC_EFUSE_23_REG(0x14f1105C) = %#x", EFUSE_U32(0x5cU));
        printk("ZXIC_EFUSE_24_REG(0x14f11060) = %#x", EFUSE_U32(0x60U));
        printk("ZXIC_EFUSE_25_REG(0x14f11064) = %#x", EFUSE_U32(0x64U));
        printk("ZXIC_EFUSE_26_REG(0x14f11068) = %#x", EFUSE_U32(0x68U));
        printk("ZXIC_EFUSE_27_REG(0x14f1106C) = %#x", EFUSE_U32(0x6cU));
        printk("ZXIC_EFUSE_28_REG(0x14f11070) = %#x", EFUSE_U32(0x70U));
        printk("ZXIC_EFUSE_29_REG(0x14f11074) = %#x", EFUSE_U32(0x74U));
        printk("ZXIC_EFUSE_30_REG(0x14f11078) = %#x", EFUSE_U32(0x78U));
        printk("ZXIC_EFUSE_31_REG(0x14f1107C) = %#x", EFUSE_U32(0x7cU));
        printk("ZXIC_EFUSE_CONFIG(0x14f11080) = %#x", EFUSE_U32(0x80U));

        printk("==================   ATE   ==================\n");
        printk("crc_ate_cp1 = %#x\n", EFUSE_U32(0x0U) & 0xffU);
        printk("x_addr      = %#x\n", (EFUSE_U32(0x0U) >> 8) & 0xffU);
        printk("y_addr      = %#x\n", (EFUSE_U32(0x0U) >> 16) & 0xffU);
        word0 = EFUSE_U32(0x8U);
        word1 = EFUSE_U32(0x4U);
        word2 = EFUSE_U32(0x0U);
        printk("lot_id      = 0x%02x%x%02x", word0 & 0xffU, word1,
               word2 >> 24);
        printk("wafer_no    = %#x\n", (EFUSE_U32(0x8U) >> 8) & 0x1fU);
        word0 = EFUSE_U32(0x8U);
        word1 = EFUSE_U32(0xcU);
        printk("crc_ate_ft1 = %#x\n", ((word1 & 0x1fU) << 3) | (word0 >> 29));
        word0 = EFUSE_U32(0xcU);
        word1 = EFUSE_U32(0x10U);
        printk("system_time = %#x\n", (word1 << 8) | (word0 >> 24));
        word0 = EFUSE_U32(0x18U);
        word1 = EFUSE_U32(0x14U);
        word2 = EFUSE_U32(0x10U);
        printk("host_name   = 0x%02x%x%02x", word0 & 0xffU, word1,
               word2 >> 24);
        printk("chip_version= %#x\n", (EFUSE_U32(0x18U) >> 8) & 0xfU);
        printk("FT_write_pw = %#x\n", EFUSE_U32(0x18U) >> 31);

        printk("==================  BOARD  ==================\n");
        printk("Safety Boot Enable     = %#x\n", EFUSE_U32(0x1cU) & 1U);
        printk("CPU JTAG Protect       = %#x\n",
               (EFUSE_U32(0x1cU) >> 1) & 1U);
        printk("Safety Boot Protect    = %#x\n",
               (EFUSE_U32(0x1cU) >> 2) & 1U);
        printk("Safety Boot AES Secret Key Write Protect = %#x\n",
               (EFUSE_U32(0x1cU) >> 3) & 1U);
        printk("Safety Boot AES Secret Key Read  Protect = %#x\n",
               (EFUSE_U32(0x1cU) >> 4) & 1U);
        printk("HUK Write Protect  = %#x\n", (EFUSE_U32(0x1cU) >> 5) & 1U);
        printk("HUK Read Protect   = %#x\n", (EFUSE_U32(0x1cU) >> 6) & 1U);
        printk("Hash Key Protect = %#x\n", (EFUSE_U32(0x1cU) >> 7) & 1U);
        printk("Reserved_protect_write = %#x\n",
               (EFUSE_U32(0x1cU) >> 8) & 1U);
        word0 = EFUSE_U32(0x2cU);
        word1 = EFUSE_U32(0x28U);
        word2 = EFUSE_U32(0x24U);
        word3 = EFUSE_U32(0x20U);
        printk("AES Secret Key        : %#x %#x %#x %#x\n", word0, word1,
               word2, word3);
        word0 = EFUSE_U32(0x3cU);
        word1 = EFUSE_U32(0x38U);
        word2 = EFUSE_U32(0x34U);
        word3 = EFUSE_U32(0x30U);
        printk("HUK(Hardware Unique Key) : %#x %#x %#x %#x\n", word0, word1,
               word2, word3);
        word0 = EFUSE_U32(0x5cU);
        word1 = EFUSE_U32(0x58U);
        word2 = EFUSE_U32(0x54U);
        word3 = EFUSE_U32(0x50U);
        word4 = EFUSE_U32(0x4cU);
        word5 = EFUSE_U32(0x48U);
        word6 = EFUSE_U32(0x44U);
        word7 = EFUSE_U32(0x40U);
        printk("HASH Key : %#x %#x %#x %#x %#x %#x %#x %#x\n", word0, word1,
               word2, word3, word4, word5, word6, word7);
        word0 = EFUSE_U32(0x64U);
        word1 = EFUSE_U32(0x60U);
        printk("Anti Rollback Counter : %#x %#x\n", word0, word1);
        printk("chip_status = %#x\n", EFUSE_U32(0x68U) & 0xffU);

        printk("===============  LEFT_PROTECT  ==============\n");
        printk("Reserved0_protect_write  = %#x\n", EFUSE_U32(0x6cU) & 1U);
        printk("Reserved1_protect_write  = %#x\n",
               (EFUSE_U32(0x6cU) >> 1) & 1U);
        printk("Reserved2_protect_write  = %#x\n",
               (EFUSE_U32(0x6cU) >> 2) & 1U);
        printk("Reserved3_protect_write  = %#x\n",
               (EFUSE_U32(0x6cU) >> 3) & 1U);
        printk("Reserved4_protect_write  = %#x\n",
               (EFUSE_U32(0x6cU) >> 4) & 1U);
        printk("Reserved5_protect_write  = %#x\n",
               (EFUSE_U32(0x6cU) >> 5) & 1U);
        printk("Reserved6_protect_write  = %#x\n",
               (EFUSE_U32(0x6cU) >> 6) & 1U);
        printk("Reserved7_protect_write  = %#x\n",
               (EFUSE_U32(0x6cU) >> 7) & 1U);
        printk("Reserved8_protect_write  = %#x\n",
               (EFUSE_U32(0x6cU) >> 8) & 1U);
        printk("Reserved9_protect_write  = %#x\n",
               (EFUSE_U32(0x6cU) >> 9) & 1U);
        printk("Reserved10_protect_write = %#x\n",
               (EFUSE_U32(0x6cU) >> 10) & 1U);
        printk("Reserved11_protect_write = %#x\n",
               (EFUSE_U32(0x6cU) >> 11) & 1U);
        printk("Reserved12_protect_write = %#x\n",
               (EFUSE_U32(0x6cU) >> 12) & 1U);
        printk("Reserved13_protect_write = %#x\n",
               (EFUSE_U32(0x6cU) >> 13) & 1U);
        printk("Reserved14_protect_write = %#x\n",
               (EFUSE_U32(0x6cU) >> 14) & 1U);

        printk("==================  ATE IP  =================\n");
        printk("T_TRIM     = %#x\n", EFUSE_U32(0x70U) & 0x1fU);
        printk("POR        = %#x\n", (EFUSE_U32(0x70U) >> 5) & 0xfU);
        printk("IDDQ_CP[11:0] = %#x\n", (EFUSE_U32(0x70U) >> 9) & 0xfffU);
        printk("Chip_Type     = %#x\n", (EFUSE_U32(0x70U) >> 21) & 0x1fU);
        printk("=============  STATUS & CONFIG  =============\n");
        printk("STATUS->EFUSE READ end  = %#x\n", EFUSE_U32(0x80U) & 1U);
        printk("config->back up aes sel = %#x\n",
               (EFUSE_U32(0x80U) >> 1) & 1U);
    } else {
        printk("================   REG VAL   ================\n");
        printk("ZXIC_EFUSE_0_REG(0x14f11000)  = %#u", EFUSE_U32(0x0U));
        printk("ZXIC_EFUSE_1_REG(0x14f11004)  = %#u", EFUSE_U32(0x4U));
        printk("ZXIC_EFUSE_2_REG(0x14f11008)  = %#u", EFUSE_U32(0x8U));
        printk("ZXIC_EFUSE_3_REG(0x14f1100C)  = %#u", EFUSE_U32(0xcU));
        printk("ZXIC_EFUSE_4_REG(0x14f11010)  = %#u", EFUSE_U32(0x10U));
        printk("ZXIC_EFUSE_5_REG(0x14f11014)  = %#u", EFUSE_U32(0x14U));
        printk("ZXIC_EFUSE_6_REG(0x14f11018)  = %#u", EFUSE_U32(0x18U));
        printk("ZXIC_EFUSE_7_REG(0x14f1101C)  = %#u", EFUSE_U32(0x1cU));
        printk("ZXIC_EFUSE_8_REG(0x14f11020)  = %#u", EFUSE_U32(0x20U));
        printk("ZXIC_EFUSE_9_REG(0x14f11024)  = %#u", EFUSE_U32(0x24U));
        printk("ZXIC_EFUSE_10_REG(0x14f11028) = %#u", EFUSE_U32(0x28U));
        printk("ZXIC_EFUSE_11_REG(0x14f1102C) = %#u", EFUSE_U32(0x2cU));
        printk("ZXIC_EFUSE_12_REG(0x14f11030) = %#u", EFUSE_U32(0x30U));
        printk("ZXIC_EFUSE_13_REG(0x14f11034) = %#u", EFUSE_U32(0x34U));
        printk("ZXIC_EFUSE_14_REG(0x14f11038) = %#u", EFUSE_U32(0x38U));
        printk("ZXIC_EFUSE_15_REG(0x14f1103C) = %#u", EFUSE_U32(0x3cU));
        printk("ZXIC_EFUSE_16_REG(0x14f11040) = %#u", EFUSE_U32(0x40U));
        printk("ZXIC_EFUSE_17_REG(0x14f11044) = %#u", EFUSE_U32(0x44U));
        printk("ZXIC_EFUSE_18_REG(0x14f11048) = %#u", EFUSE_U32(0x48U));
        printk("ZXIC_EFUSE_19_REG(0x14f1104C) = %#u", EFUSE_U32(0x4cU));
        printk("ZXIC_EFUSE_20_REG(0x14f11050) = %#u", EFUSE_U32(0x50U));
        printk("ZXIC_EFUSE_21_REG(0x14f11054) = %#u", EFUSE_U32(0x54U));
        printk("ZXIC_EFUSE_22_REG(0x14f11058) = %#u", EFUSE_U32(0x58U));
        printk("ZXIC_EFUSE_23_REG(0x14f1105C) = %#u", EFUSE_U32(0x5cU));
        printk("ZXIC_EFUSE_24_REG(0x14f11060) = %#u", EFUSE_U32(0x60U));
        printk("ZXIC_EFUSE_25_REG(0x14f11064) = %#u", EFUSE_U32(0x64U));
        printk("ZXIC_EFUSE_26_REG(0x14f11068) = %#u", EFUSE_U32(0x68U));
        printk("ZXIC_EFUSE_27_REG(0x14f1106C) = %#u", EFUSE_U32(0x6cU));
        printk("ZXIC_EFUSE_28_REG(0x14f11070) = %#u", EFUSE_U32(0x70U));
        printk("ZXIC_EFUSE_29_REG(0x14f11074) = %#u", EFUSE_U32(0x74U));
        printk("ZXIC_EFUSE_30_REG(0x14f11078) = %#u", EFUSE_U32(0x78U));
        printk("ZXIC_EFUSE_31_REG(0x14f1107C) = %#u", EFUSE_U32(0x7cU));
        printk("ZXIC_EFUSE_CONFIG(0x14f11080) = %#u", EFUSE_U32(0x80U));

        printk("==================   ATE   ==================\n");
        printk("crc_ate_cp1 = %#u\n", EFUSE_U32(0x0U) & 0xffU);
        printk("x_addr      = %#u\n", (EFUSE_U32(0x0U) >> 8) & 0xffU);
        printk("y_addr      = %#u\n", (EFUSE_U32(0x0U) >> 16) & 0xffU);
        word0 = EFUSE_U32(0x8U);
        word1 = EFUSE_U32(0x4U);
        word2 = EFUSE_U32(0x0U);
        printk("lot_id      = 0x%02x%x%02x", word0 & 0xffU, word1,
               word2 >> 24);
        printk("wafer_no    = %#u\n", (EFUSE_U32(0x8U) >> 8) & 0x1fU);
        word0 = EFUSE_U32(0x8U);
        word1 = EFUSE_U32(0xcU);
        printk("crc_ate_ft1 = %#u\n", ((word1 & 0x1fU) << 3) | (word0 >> 29));
        word0 = EFUSE_U32(0xcU);
        word1 = EFUSE_U32(0x10U);
        printk("system_time = %#u\n", (word1 << 8) | (word0 >> 24));
        word0 = EFUSE_U32(0x18U);
        word1 = EFUSE_U32(0x14U);
        word2 = EFUSE_U32(0x10U);
        printk("host_name   = 0x%02x%x%02x", word0 & 0xffU, word1,
               word2 >> 24);
        printk("chip_version= %#u\n", (EFUSE_U32(0x18U) >> 8) & 0xfU);
        printk("FT_write_pw = %#u\n", EFUSE_U32(0x18U) >> 31);

        printk("==================  BOARD  ==================\n");
        printk("Safety Boot Enable     = %#u\n", EFUSE_U32(0x1cU) & 1U);
        printk("CPU JTAG Protect       = %#u\n",
               (EFUSE_U32(0x1cU) >> 1) & 1U);
        printk("Safety Boot Protect    = %#u\n",
               (EFUSE_U32(0x1cU) >> 2) & 1U);
        printk("Safety Boot AES Secret Key Write Protect = %#u\n",
               (EFUSE_U32(0x1cU) >> 3) & 1U);
        printk("Safety Boot AES Secret Key Read  Protect = %#u\n",
               (EFUSE_U32(0x1cU) >> 4) & 1U);
        printk("Backup AES Secret Key  = %#u\n",
               (EFUSE_U32(0x1cU) >> 5) & 1U);
        printk("Backup AES Secret Key Protect = %#u\n",
               (EFUSE_U32(0x1cU) >> 6) & 1U);
        printk("Reserved_protect_write = %#u\n",
               (EFUSE_U32(0x1cU) >> 7) & 1U);
        word0 = EFUSE_U32(0x2cU);
        word1 = EFUSE_U32(0x28U);
        word2 = EFUSE_U32(0x24U);
        word3 = EFUSE_U32(0x20U);
        printk("AES Secret Key        : %#u %#u %#u %#u\n", word0, word1,
               word2, word3);
        word0 = EFUSE_U32(0x3cU);
        word1 = EFUSE_U32(0x38U);
        word2 = EFUSE_U32(0x34U);
        word3 = EFUSE_U32(0x30U);
        printk("Backup AES Secret Key : %#u %#u %#u %#u\n", word0, word1,
               word2, word3);

        printk("===============  LEFT_PROTECT  ==============\n");
        printk("Reserved0_protect_write  = %#u\n", EFUSE_U32(0x40U) & 1U);
        printk("Reserved1_protect_write  = %#u\n",
               (EFUSE_U32(0x40U) >> 1) & 1U);
        printk("Reserved2_protect_write  = %#u\n",
               (EFUSE_U32(0x40U) >> 2) & 1U);
        printk("Reserved3_protect_write  = %#u\n",
               (EFUSE_U32(0x40U) >> 3) & 1U);
        printk("Reserved4_protect_write  = %#u\n",
               (EFUSE_U32(0x40U) >> 4) & 1U);
        printk("Reserved5_protect_write  = %#u\n",
               (EFUSE_U32(0x40U) >> 5) & 1U);
        printk("Reserved6_protect_write  = %#u\n",
               (EFUSE_U32(0x40U) >> 6) & 1U);
        printk("Reserved7_protect_write  = %#u\n",
               (EFUSE_U32(0x40U) >> 7) & 1U);
        printk("Reserved8_protect_write  = %#u\n",
               (EFUSE_U32(0x40U) >> 8) & 1U);
        printk("Reserved9_protect_write  = %#u\n",
               (EFUSE_U32(0x40U) >> 9) & 1U);
        printk("Reserved10_protect_write = %#u\n",
               (EFUSE_U32(0x40U) >> 10) & 1U);
        printk("Reserved11_protect_write = %#u\n",
               (EFUSE_U32(0x40U) >> 11) & 1U);
        printk("Reserved12_protect_write = %#u\n",
               (EFUSE_U32(0x40U) >> 12) & 1U);
        printk("Reserved13_protect_write = %#u\n",
               (EFUSE_U32(0x40U) >> 13) & 1U);
        printk("Reserved14_protect_write = %#u\n",
               (EFUSE_U32(0x40U) >> 14) & 1U);

        printk("==================  ATE IP  =================\n");
        printk("T25_DATA   = %#u\n", EFUSE_U32(0x44U) & 0x3ffU);
        printk("T_TRIM     = %#u\n", (EFUSE_U32(0x44U) >> 10) & 0x1fU);
        printk("V_DATA     = %#u\n", (EFUSE_U32(0x44U) >> 16) & 0x3ffU);
        printk("POR        = %#u\n", (EFUSE_U32(0x44U) >> 26) & 0xfU);
        printk("P_SVT_DATA = %#u\n", EFUSE_U32(0x48U) & 0x3ffU);
        printk("P_LVT_DATA = %#u\n", (EFUSE_U32(0x48U) >> 10) & 0x3ffU);
        printk("P_HVT_DATA = %#u\n", (EFUSE_U32(0x48U) >> 20) & 0x3ffU);
        printk("PON_mode   = %#u\n", EFUSE_U32(0x48U) >> 30);
        printk("IDDQ_CP[11:0] = %#u\n", EFUSE_U32(0x4cU) & 0xfffU);
        printk("BIN_CP        = %#u\n", (EFUSE_U32(0x4cU) >> 12) & 0xffU);
        printk("Package_Type  = %#u\n", (EFUSE_U32(0x4cU) >> 20) & 0xfU);
        printk("Bin           = %#u\n", (EFUSE_U32(0x4cU) >> 24) & 0xfU);
        printk("A53_bin       = %#u\n", EFUSE_U32(0x4cU) >> 28);
        printk("IDDQ_FT1       = %#u\n", EFUSE_U32(0x50U) & 0xfffU);
        printk("IDDQ_FT2       = %#u\n", (EFUSE_U32(0x50U) >> 12) & 0xfffU);
        printk("GEPHY_temo_coef= %#u\n", (EFUSE_U32(0x50U) >> 24) & 3U);
        printk("PPU_CLK        = %#u\n", (EFUSE_U32(0x50U) >> 26) & 3U);
        printk("=============  STATUS & CONFIG  =============\n");
        printk("STATUS->EFUSE READ end  = %#u\n", EFUSE_U32(0x80U) & 1U);
        printk("config->back up aes sel = %#u\n",
               (EFUSE_U32(0x80U) >> 1) & 1U);
    }

    return 0;
}

int serdes_set_tx_eq(uint32_t tx_eq)
{
    volatile uint32_t *tx_eq_register;

    if (tx_eq == 0U) {
        tx_eq_register = (volatile uint32_t *)(pon_serdes_base + 0x20U);
        *tx_eq_register = (*tx_eq_register & 0xffff00ffU) | 0x0d00U;
        printk("\nset tx 3db pre and post success \n");
    } else if (tx_eq == 1U) {
        tx_eq_register = (volatile uint32_t *)(pon_serdes_base + 0x20U);
        *tx_eq_register = (*tx_eq_register & 0xffff00ffU) | 0x1d00U;
        printk("\nset tx 6db pre and post success \n");
    }

    return 0;
}

int serdes_set_pll_open_loop(uint32_t enable)
{
    volatile uint32_t *pll_control_register;
    volatile uint32_t *open_loop_register;
    uint32_t pll_control;

    pll_control_register = (volatile uint32_t *)(pon_serdes_base + 0x68U);
    open_loop_register = (volatile uint32_t *)(pon_serdes_base + 0x74U);
    pll_control = *pll_control_register;

    if (enable == 1U) {
        *pll_control_register = pll_control | 0x600U;
        *pll_control_register |= 0x400000U;
        *open_loop_register = (*open_loop_register & 0xffff9fffU) | 0x2000U;
        return printk("open pll open loop en =0x%x\n", enable);
    }

    *pll_control_register = pll_control & 0xfffff9ffU;
    *pll_control_register &= 0xffbfffffU;
    *open_loop_register &= 0xffff9fffU;
    return printk("close pll open loop en =0x%x\n", enable);
}

int serdes_set_clk_change(uint32_t use_local_clock)
{
    volatile uint32_t *clock_select_register;

    clock_select_register = (volatile uint32_t *)(pon_serdes_base + 0x48U);
    if (use_local_clock == 0U) {
        *clock_select_register &= 0xfffbffffU;
        return printk("set rx to tx  clk looptiming clk\n");
    }

    *clock_select_register |= 0x40000U;
    return printk("set rx to tx  clk local clk\n");
}

void serdes_set_rx_eq1(uint32_t enable, uint32_t equalizer_value)
{
    volatile uint32_t *equalizer_register;

    if (enable > 1U) {
        printk("serdes rx eq1 en is too big\n");
        return;
    }

    equalizer_register = (volatile uint32_t *)(pon_serdes_base + 0x2cU);
    if (enable == 0U) {
        *equalizer_register |= 1U;
        return;
    }

    *equalizer_register &= 0xfffffffeU;
    *equalizer_register =
        (*equalizer_register & 0xffffff07U) | (equalizer_value << 3);
}

void serdes_set_rx_eq2(uint32_t enable, uint32_t equalizer_value)
{
    volatile uint32_t *equalizer_register;

    if (enable > 1U) {
        printk("serdes rx eq2 en is too big\n");
        return;
    }

    equalizer_register = (volatile uint32_t *)(pon_serdes_base + 0x2cU);
    if (enable == 0U) {
        *equalizer_register |= 2U;
        return;
    }

    *equalizer_register &= 0xfffffffdU;
    *equalizer_register =
        (*equalizer_register & 0xffffe0ffU) | (equalizer_value << 8);
}

void serdes_set_rx_eq3(uint32_t enable, uint32_t equalizer_value)
{
    volatile uint32_t *equalizer_register;

    if (enable > 1U) {
        printk("serdes rx eq3 en is too big\n");
        return;
    }

    equalizer_register = (volatile uint32_t *)(pon_serdes_base + 0x2cU);
    if (enable == 0U) {
        *equalizer_register |= 4U;
        return;
    }

    *equalizer_register &= 0xfffffffbU;
    *equalizer_register =
        (*equalizer_register & 0xfffc1fffU) | (equalizer_value << 13);
}

void serdes_set_lane_mode(uint32_t lane_mode)
{
    volatile uint32_t *lane_mode_register;

    if (isCpuType_133() != 1U)
        return;

    lane_mode_register = (volatile uint32_t *)(pon_serdes_base + 0x94U);
    *lane_mode_register = (*lane_mode_register & 0xfffffff8U) | lane_mode;
}

int serdes_set_error_time_en(uint32_t enable)
{
    volatile uint32_t *error_time_register;

    error_time_register = (volatile uint32_t *)(pon_serdes_base + 0x94U);
    *error_time_register =
        (*error_time_register & 0xbfffffffU) | (enable << 30);
    return printk("set error time is ok\n");
}

int serdes_get_hard_prbs_cnt(uint32_t seconds, uint32_t prbs_mode)
{
    uint32_t delay_count;
    uint64_t delay_index;

    serdes_err_cnt_reset();
    serdes_set_error_time(seconds);
    serdes_set_rx_prbs_mode(prbs_mode);
    serdes_set_check_en(1U);
    serdes_set_err_cnt_en(1U);
    serdes_set_error_time_en(1U);

    delay_count = seconds * 1000U;
    delay_index = 0U;
    while (delay_index != (uint64_t)delay_count) {
        ++delay_index;
        __const_udelay(0x418958UL);
    }

    iPrbsCounter += serdes_get_err_cnt();
    return printk("serdes_get_hard_prbs_cnt counter: %ld.\n",
                  (long)iPrbsCounter);
}

int serdes_get_prbs_counters(int time)
{
    uint32_t delay_ticks;

    printk("%s time = %d\n", "serdes_get_prbs_counters", time);
    serdes_set_error_time_en(0U);
    if (los_state_prbs != 0U) {
        printk("PON_SERDES PonGetPrbsCounters Error: RXBIST is not locked, "
               "please check the configuration\n");
        return 0;
    }

    del_timer(&serdes_prbs_counter_timer);
    init_timer_key(&serdes_prbs_counter_timer, serdesPrbsCounterGetHandler,
                   0U, 0U, 0U);
    delay_ticks = (uint32_t)time * 100U;
    serdes_prbs_counter_timer.expires = jiffies + delay_ticks;
    add_timer(&serdes_prbs_counter_timer);
    serdes_err_cnt_reset();
    __const_udelay(0x418958UL);
    serdesPrbsCounter = serdes_get_err_cnt();
    return 0;
}

void mode_epon_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_epon_cfg\n");
    if (isCpuType_132() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x2af8c874U;
        registers[0x4U / 4U] = 0x41d124a0U;
        registers[0x8U / 4U] = 0x0105c787U;
        registers[0xcU / 4U] = 0x19000200U;
        registers[0x10U / 4U] = 0x21965008U;
        registers[0x14U / 4U] = 0x00190002U;
        registers[0x18U / 4U] = 0x00000800U;
        registers[0x1cU / 4U] = 0x00020125U;
        registers[0x20U / 4U] = 0x8f000000U;
        registers[0x24U / 4U] = 0x0045000aU;
        registers[0x28U / 4U] = 0x0c633930U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x03a81140U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x010058eaU;
        registers[0x4cU / 4U] = 0x26292210U;
        registers[0x50U / 4U] = 0x00000029U;
        registers[0x54U / 4U] = 0x00000000U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200514a8U;
        registers[0x64U / 4U] = 0x00000091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x00003a00U;
        registers[0x70U / 4U] = 0x005fff0fU;
        registers[0x74U / 4U] = 0x00000002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0x00000048U;
        registers[0x80U / 4U] = 0x00000000U;
        registers[0x84U / 4U] = 0x00000400U;
        registers[0x88U / 4U] = 0xffff0000U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x00000000U;
        registers[0x98U / 4U] = 0xa352943fU;
        registers[0x9cU / 4U] = 0x33333333U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x03003333U;
        registers[0xa8U / 4U] = 0x00000818U;
        registers[0xacU / 4U] = 0x0000000dU;
        registers[0xb0U / 4U] = 0x00000000U;
        registers[0xb4U / 4U] = 0x00000000U;
    } else {
        if (isCpuType_133() != 1U)
            return;

        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0xe0000004U;
        registers[0x4U / 4U] = 0x4fa840a1U;
        registers[0x8U / 4U] = 0x013e860fU;
        registers[0xcU / 4U] = 0x0210c073U;
        registers[0x10U / 4U] = 0x00000048U;
        registers[0x14U / 4U] = 0x00000000U;
        registers[0x18U / 4U] = 0x00ff8000U;
        registers[0x1cU / 4U] = 0x00238720U;
        registers[0x20U / 4U] = 0x80000000U;
        registers[0x24U / 4U] = 0x00020000U;
        registers[0x28U / 4U] = 0x0c633830U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0xc5a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x01002a2aU;
        registers[0x4cU / 4U] = 0x66002200U;
        registers[0x50U / 4U] = 0x34000003U;
        registers[0x54U / 4U] = 0x00000100U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x00648091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x00003700U;
        registers[0x70U / 4U] = 0xa9404000U;
        registers[0x74U / 4U] = 0x10640002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x103f103fU;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x80000007U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x55555500U;
        registers[0xa0U / 4U] = 0x55555555U;
        registers[0xa4U / 4U] = 0x00555555U;
        registers[0xa8U / 4U] = 0x30000818U;
        registers[0xacU / 4U] = 0x40002000U;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    }

    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void mode_10g_epon_nsyn_dpll_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_10g_epon_nsyn_dpll_cfg\n");
    if (isCpuType_132() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x2af8c874U;
        registers[0x4U / 4U] = 0x41d124a0U;
        registers[0x8U / 4U] = 0x0145c787U;
        registers[0xcU / 4U] = 0x04000300U;
        registers[0x10U / 4U] = 0x21965008U;
        registers[0x14U / 4U] = 0x00190002U;
        registers[0x18U / 4U] = 0x00000800U;
        registers[0x1cU / 4U] = 0x00020425U;
        registers[0x20U / 4U] = 0x8f000000U;
        registers[0x24U / 4U] = 0x0045000aU;
        registers[0x28U / 4U] = 0x0c633930U;
        registers[0x2cU / 4U] = 0x00000aa8U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x04681140U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x8f04281aU;
        registers[0x4cU / 4U] = 0xa00d2230U;
        registers[0x50U / 4U] = 0x0000000dU;
        registers[0x54U / 4U] = 0x00000400U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x00002013U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x00003a00U;
        registers[0x70U / 4U] = 0x005fff0fU;
        registers[0x74U / 4U] = 0x00000002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0x00000048U;
        registers[0x80U / 4U] = 0x00000000U;
        registers[0x84U / 4U] = 0x00000400U;
        registers[0x88U / 4U] = 0xffff0000U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000005cU;
        registers[0x94U / 4U] = 0x00000100U;
        registers[0x98U / 4U] = 0xa352943fU;
        registers[0x9cU / 4U] = 0x33333333U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x03003333U;
        registers[0xa8U / 4U] = 0x00000818U;
        registers[0xacU / 4U] = 0x0000000dU;
        registers[0xb0U / 4U] = 0x00000000U;
        registers[0xb4U / 4U] = 0x00000000U;
    } else {
        if (isCpuType_133() != 1U)
            return;

        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0xe0000004U;
        registers[0x4U / 4U] = 0x50a840a1U;
        registers[0x8U / 4U] = 0x013e864fU;
        registers[0xcU / 4U] = 0x0418c1f3U;
        registers[0x10U / 4U] = 0x0000004cU;
        registers[0x14U / 4U] = 0x00290000U;
        registers[0x18U / 4U] = 0x00ff8000U;
        registers[0x1cU / 4U] = 0x00238220U;
        registers[0x20U / 4U] = 0x80000100U;
        registers[0x24U / 4U] = 0x00050000U;
        registers[0x28U / 4U] = 0x0c633830U;
        registers[0x2cU / 4U] = 0x00000e46U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x05a85100U;
        registers[0x44U / 4U] = 0x02000000U;
        registers[0x48U / 4U] = 0x4f002b2aU;
        registers[0x4cU / 4U] = 0x60002220U;
        registers[0x50U / 4U] = 0x34000003U;
        registers[0x54U / 4U] = 0x00000d00U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200574a8U;
        registers[0x64U / 4U] = 0x67748093U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x40003f00U;
        registers[0x70U / 4U] = 0xa9404000U;
        registers[0x74U / 4U] = 0x10108002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x103f103fU;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000201cU;
        registers[0x94U / 4U] = 0x8028e100U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x55555500U;
        registers[0xa0U / 4U] = 0x55555555U;
        registers[0xa4U / 4U] = 0x00555555U;
        registers[0xa8U / 4U] = 0x30000818U;
        registers[0xacU / 4U] = 0x40002000U;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    }

    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void mode_10g_epon_nsyn_fifo_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_10g_epon_nsyn_fifo_cfg\n");
    if (isCpuType_132() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x2af8c874U;
        registers[0x4U / 4U] = 0x41d124a0U;
        registers[0x8U / 4U] = 0x0145c787U;
        registers[0xcU / 4U] = 0x19000200U;
        registers[0x10U / 4U] = 0x21965008U;
        registers[0x14U / 4U] = 0x00190002U;
        registers[0x18U / 4U] = 0x00000800U;
        registers[0x1cU / 4U] = 0x00020725U;
        registers[0x20U / 4U] = 0x8f000000U;
        registers[0x24U / 4U] = 0x0045000aU;
        registers[0x28U / 4U] = 0x0c633930U;
        registers[0x2cU / 4U] = 0x00000aa8U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x0c681140U;
        registers[0x44U / 4U] = 0x02000000U;
        registers[0x48U / 4U] = 0x8f00281aU;
        registers[0x4cU / 4U] = 0xa00d2210U;
        registers[0x50U / 4U] = 0x0000000dU;
        registers[0x54U / 4U] = 0x00000000U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200514a8U;
        registers[0x64U / 4U] = 0x00000091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x00003a00U;
        registers[0x70U / 4U] = 0x005fff0fU;
        registers[0x74U / 4U] = 0x00000002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0x00000048U;
        registers[0x80U / 4U] = 0x00000000U;
        registers[0x84U / 4U] = 0x00000400U;
        registers[0x88U / 4U] = 0xffff0000U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000005cU;
        registers[0x94U / 4U] = 0x00000100U;
        registers[0x98U / 4U] = 0xa352943fU;
        registers[0x9cU / 4U] = 0x33333333U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x03003333U;
        registers[0xa8U / 4U] = 0x00000818U;
        registers[0xacU / 4U] = 0x0000000dU;
        registers[0xb0U / 4U] = 0x00000000U;
        registers[0xb4U / 4U] = 0x00000000U;
    } else {
        if (isCpuType_133() != 1U)
            return;

        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0xe0000004U;
        registers[0x4U / 4U] = 0x50a840a1U;
        registers[0x8U / 4U] = 0x013e864fU;
        registers[0xcU / 4U] = 0x0210c073U;
        registers[0x10U / 4U] = 0x00000048U;
        registers[0x14U / 4U] = 0x00000000U;
        registers[0x18U / 4U] = 0x00ff8000U;
        registers[0x1cU / 4U] = 0x00230720U;
        registers[0x20U / 4U] = 0x80000100U;
        registers[0x24U / 4U] = 0x00050000U;
        registers[0x28U / 4U] = 0x0c633830U;
        registers[0x2cU / 4U] = 0x00000e46U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x0da85100U;
        registers[0x44U / 4U] = 0x02000000U;
        registers[0x48U / 4U] = 0x4f002b2aU;
        registers[0x4cU / 4U] = 0x60002200U;
        registers[0x50U / 4U] = 0x34000003U;
        registers[0x54U / 4U] = 0x00000100U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x67748091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x40003f00U;
        registers[0x70U / 4U] = 0xa9404000U;
        registers[0x74U / 4U] = 0x10670002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x103f103fU;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x8028c100U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x55555500U;
        registers[0xa0U / 4U] = 0x55555555U;
        registers[0xa4U / 4U] = 0x00555555U;
        registers[0xa8U / 4U] = 0x30000818U;
        registers[0xacU / 4U] = 0x40002000U;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    }

    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void mode_10g_epon_nsyn_nofifo_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_10g_epon_nsyn_nofifo_cfg\n");
    if (isCpuType_132() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x2af8c874U;
        registers[0x4U / 4U] = 0x41d124a0U;
        registers[0x8U / 4U] = 0x0145c787U;
        registers[0xcU / 4U] = 0x19000200U;
        registers[0x10U / 4U] = 0x21965008U;
        registers[0x14U / 4U] = 0x00190002U;
        registers[0x18U / 4U] = 0x00000800U;
        registers[0x1cU / 4U] = 0x00020725U;
        registers[0x20U / 4U] = 0x8f000000U;
        registers[0x24U / 4U] = 0x0045000aU;
        registers[0x28U / 4U] = 0x0c633930U;
        registers[0x2cU / 4U] = 0x00000aa8U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x04681140U;
        registers[0x44U / 4U] = 0x01f80000U;
        registers[0x48U / 4U] = 0x8f00281aU;
        registers[0x4cU / 4U] = 0xa00d2210U;
        registers[0x50U / 4U] = 0x0000000dU;
        registers[0x54U / 4U] = 0x00000000U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200514a8U;
        registers[0x64U / 4U] = 0x00000091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x00003a00U;
        registers[0x70U / 4U] = 0x005fff0fU;
        registers[0x74U / 4U] = 0x00000002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0x00000048U;
        registers[0x80U / 4U] = 0x00000000U;
        registers[0x84U / 4U] = 0x00000400U;
        registers[0x88U / 4U] = 0xffff0000U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000005cU;
        registers[0x94U / 4U] = 0x00000100U;
        registers[0x98U / 4U] = 0xa352943fU;
        registers[0x9cU / 4U] = 0x33333333U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x03003333U;
        registers[0xa8U / 4U] = 0x00000818U;
        registers[0xacU / 4U] = 0x0000000dU;
        registers[0xb0U / 4U] = 0x00000000U;
        registers[0xb4U / 4U] = 0x00000000U;
    } else {
        if (isCpuType_133() != 1U)
            return;

        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0xe0000004U;
        registers[0x4U / 4U] = 0x50a840a1U;
        registers[0x8U / 4U] = 0x013e864fU;
        registers[0xcU / 4U] = 0x0210c073U;
        registers[0x10U / 4U] = 0x00000048U;
        registers[0x14U / 4U] = 0x00000000U;
        registers[0x18U / 4U] = 0x00ff8000U;
        registers[0x1cU / 4U] = 0x00238720U;
        registers[0x20U / 4U] = 0x16000000U;
        registers[0x24U / 4U] = 0x00050000U;
        registers[0x28U / 4U] = 0x0c633830U;
        registers[0x2cU / 4U] = 0x00000e46U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x05a85100U;
        registers[0x44U / 4U] = 0x01f16000U;
        registers[0x48U / 4U] = 0x4f002b2aU;
        registers[0x4cU / 4U] = 0x60002200U;
        registers[0x50U / 4U] = 0x34000003U;
        registers[0x54U / 4U] = 0x00000100U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x67748091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x40003f00U;
        registers[0x70U / 4U] = 0xa9404000U;
        registers[0x74U / 4U] = 0x10670002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x103f103fU;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x8028e100U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x55555500U;
        registers[0xa0U / 4U] = 0x55555555U;
        registers[0xa4U / 4U] = 0x00555555U;
        registers[0xa8U / 4U] = 0x30000818U;
        registers[0xacU / 4U] = 0x40002000U;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    }

    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void mode_10g_epon_syn_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_10g_epon_syn_cfg\n");
    if (isCpuType_133() != 1U)
        return;

    registers = (volatile uint32_t *)pon_serdes_base;
    registers[0x0U / 4U] = 0xe0000004U;
    registers[0x4U / 4U] = 0x50a840a1U;
    registers[0x8U / 4U] = 0x013e864fU;
    registers[0xcU / 4U] = 0x0210c073U;
    registers[0x10U / 4U] = 0x00000048U;
    registers[0x14U / 4U] = 0x00000000U;
    registers[0x18U / 4U] = 0x00ff8000U;
    registers[0x1cU / 4U] = 0x00238120U;
    registers[0x20U / 4U] = 0x80000500U;
    registers[0x24U / 4U] = 0x00020000U;
    registers[0x28U / 4U] = 0x0c633830U;
    registers[0x2cU / 4U] = 0x00000e46U;
    registers[0x30U / 4U] = 0x00000020U;
    registers[0x34U / 4U] = 0x00002000U;
    registers[0x38U / 4U] = 0x00000000U;
    registers[0x3cU / 4U] = 0x00ff0000U;
    registers[0x40U / 4U] = 0x05a85100U;
    registers[0x44U / 4U] = 0x00000000U;
    registers[0x48U / 4U] = 0x4f002b6aU;
    registers[0x4cU / 4U] = 0x60002200U;
    registers[0x50U / 4U] = 0x34000003U;
    registers[0x54U / 4U] = 0x00000100U;
    registers[0x58U / 4U] = 0x00000080U;
    registers[0x5cU / 4U] = 0x00000000U;
    registers[0x60U / 4U] = 0x200554a8U;
    registers[0x64U / 4U] = 0x67748091U;
    registers[0x68U / 4U] = 0x01000000U;
    registers[0x6cU / 4U] = 0x40003f00U;
    registers[0x70U / 4U] = 0xa9404000U;
    registers[0x74U / 4U] = 0x10670002U;
    registers[0x78U / 4U] = 0x000000daU;
    registers[0x7cU / 4U] = 0xf20000c8U;
    registers[0x80U / 4U] = 0x10381038U;
    registers[0x84U / 4U] = 0x01000200U;
    registers[0x88U / 4U] = 0x00020001U;
    registers[0x8cU / 4U] = 0x10000000U;
    registers[0x90U / 4U] = 0x0000001cU;
    registers[0x94U / 4U] = 0x802dc500U;
    registers[0x98U / 4U] = 0x000000ffU;
    registers[0x9cU / 4U] = 0x55555500U;
    registers[0xa0U / 4U] = 0x55555555U;
    registers[0xa4U / 4U] = 0x00555555U;
    registers[0xa8U / 4U] = 0x30000818U;
    registers[0xacU / 4U] = 0x40002000U;
    registers[0xb0U / 4U] = 0x0000000cU;
    registers[0xb4U / 4U] = 0x01000000U;
    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void mode_gpon_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_gpon_cfg\n");
    if (isCpuType_129() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x00070004U;
        registers[0x4U / 4U] = 0x000040a0U;
        registers[0x8U / 4U] = 0x00008007U;
        registers[0xcU / 4U] = 0x18c00000U;
        registers[0x10U / 4U] = 0x21965404U;
        registers[0x14U / 4U] = 0x00194002U;
        registers[0x18U / 4U] = 0x00080000U;
        registers[0x1cU / 4U] = 0x00000320U;
        registers[0x20U / 4U] = 0x80000200U;
        registers[0x24U / 4U] = 0x00020000U;
        registers[0x28U / 4U] = 0x00000010U;
        registers[0x2cU / 4U] = 0x00000421U;
        registers[0x30U / 4U] = 0x00000000U;
        registers[0x34U / 4U] = 0x00000000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00000000U;
        registers[0x40U / 4U] = 0x05a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x03002aeaU;
        registers[0x4cU / 4U] = 0x80002200U;
        registers[0x50U / 4U] = 0x00000604U;
        registers[0x54U / 4U] = 0x00000400U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x3de49092U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0xc0003700U;
        registers[0x70U / 4U] = 0xa9004000U;
        registers[0x74U / 4U] = 0x10108000U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x10371037U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x00000000U;
        registers[0x94U / 4U] = 0x80000100U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x33333300U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x00333333U;
        registers[0xa8U / 4U] = 0x20000818U;
        registers[0xacU / 4U] = 0x0000201cU;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    } else if (isCpuType_132() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x2af8c874U;
        registers[0x4U / 4U] = 0x41d124a0U;
        registers[0x8U / 4U] = 0x0105c787U;
        registers[0xcU / 4U] = 0x18d80200U;
        registers[0x10U / 4U] = 0x21965008U;
        registers[0x14U / 4U] = 0x00190002U;
        registers[0x18U / 4U] = 0x00000800U;
        registers[0x1cU / 4U] = 0x00020325U;
        registers[0x20U / 4U] = 0x8f000000U;
        registers[0x24U / 4U] = 0x0047000aU;
        registers[0x28U / 4U] = 0x0c633930U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x03a81140U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x030068eaU;
        registers[0x4cU / 4U] = 0x24092210U;
        registers[0x50U / 4U] = 0x00000009U;
        registers[0x54U / 4U] = 0x00000000U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200514a8U;
        registers[0x64U / 4U] = 0x00000091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x00003a00U;
        registers[0x70U / 4U] = 0x005fff0fU;
        registers[0x74U / 4U] = 0x00000002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0x00000048U;
        registers[0x80U / 4U] = 0x00000000U;
        registers[0x84U / 4U] = 0x00000400U;
        registers[0x88U / 4U] = 0xffff0000U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x00000000U;
        registers[0x98U / 4U] = 0xa352943fU;
        registers[0x9cU / 4U] = 0x33333333U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x03003333U;
        registers[0xa8U / 4U] = 0x00000818U;
        registers[0xacU / 4U] = 0x0000000dU;
        registers[0xb0U / 4U] = 0x00000000U;
        registers[0xb4U / 4U] = 0x00000000U;
    } else {
        if (isCpuType_133() != 1U)
            return;

        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0xe0000004U;
        registers[0x4U / 4U] = 0x50a840a1U;
        registers[0x8U / 4U] = 0x013e860fU;
        registers[0xcU / 4U] = 0x0210c073U;
        registers[0x10U / 4U] = 0x00000048U;
        registers[0x14U / 4U] = 0x00000000U;
        registers[0x18U / 4U] = 0x00ff8000U;
        registers[0x1cU / 4U] = 0x00238720U;
        registers[0x20U / 4U] = 0x80000300U;
        registers[0x24U / 4U] = 0x00070000U;
        registers[0x28U / 4U] = 0x0c633830U;
        registers[0x2cU / 4U] = 0x00000e46U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x85a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x03006b2aU;
        registers[0x4cU / 4U] = 0x64002200U;
        registers[0x50U / 4U] = 0x34000003U;
        registers[0x54U / 4U] = 0x00000100U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x63f48091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x10cb3f00U;
        registers[0x70U / 4U] = 0xa9404000U;
        registers[0x74U / 4U] = 0x10670002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x103f103fU;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x8028c000U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x55555500U;
        registers[0xa0U / 4U] = 0x55555555U;
        registers[0xa4U / 4U] = 0x00555555U;
        registers[0xa8U / 4U] = 0x30000818U;
        registers[0xacU / 4U] = 0x40002000U;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    }

    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void mode_gpon_syn_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_gpon_cfg\n");
    if (isCpuType_129() != 1U)
        return;

    registers = (volatile uint32_t *)pon_serdes_base;
    registers[0x0U / 4U] = 0x00070004U;
    registers[0x4U / 4U] = 0x000040a0U;
    registers[0x8U / 4U] = 0x00008007U;
    registers[0xcU / 4U] = 0x18c00000U;
    registers[0x10U / 4U] = 0x21965404U;
    registers[0x14U / 4U] = 0x00194002U;
    registers[0x18U / 4U] = 0x00080000U;
    registers[0x1cU / 4U] = 0x00000120U;
    registers[0x20U / 4U] = 0x80000200U;
    registers[0x24U / 4U] = 0x00020000U;
    registers[0x28U / 4U] = 0x00000010U;
    registers[0x2cU / 4U] = 0x00000421U;
    registers[0x30U / 4U] = 0x00000000U;
    registers[0x34U / 4U] = 0x00000000U;
    registers[0x38U / 4U] = 0x00000000U;
    registers[0x3cU / 4U] = 0x00000000U;
    registers[0x40U / 4U] = 0x05a85100U;
    registers[0x44U / 4U] = 0x00000000U;
    registers[0x48U / 4U] = 0x03002aeaU;
    registers[0x4cU / 4U] = 0x80002200U;
    registers[0x50U / 4U] = 0x00000604U;
    registers[0x54U / 4U] = 0x00000400U;
    registers[0x58U / 4U] = 0x00000080U;
    registers[0x5cU / 4U] = 0x00000000U;
    registers[0x60U / 4U] = 0x200554a8U;
    registers[0x64U / 4U] = 0x3de49092U;
    registers[0x68U / 4U] = 0x01000000U;
    registers[0x6cU / 4U] = 0xc0003700U;
    registers[0x70U / 4U] = 0xa9004000U;
    registers[0x74U / 4U] = 0x10108000U;
    registers[0x78U / 4U] = 0x000000daU;
    registers[0x7cU / 4U] = 0xf20000c8U;
    registers[0x80U / 4U] = 0x10371037U;
    registers[0x84U / 4U] = 0x01000200U;
    registers[0x88U / 4U] = 0x00020001U;
    registers[0x8cU / 4U] = 0x10000000U;
    registers[0x90U / 4U] = 0x00000000U;
    registers[0x94U / 4U] = 0x80000500U;
    registers[0x98U / 4U] = 0x000000ffU;
    registers[0x9cU / 4U] = 0x33333300U;
    registers[0xa0U / 4U] = 0x33333333U;
    registers[0xa4U / 4U] = 0x00333333U;
    registers[0xa8U / 4U] = 0x20000818U;
    registers[0xacU / 4U] = 0x0000201cU;
    registers[0xb0U / 4U] = 0x0000000cU;
    registers[0xb4U / 4U] = 0x01000000U;
    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void mode_xgpon_nsyn_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_xgpon_nsyn_cfg\n");
    if (isCpuType_132() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x2af8c874U;
        registers[0x4U / 4U] = 0x41d124a0U;
        registers[0x8U / 4U] = 0x0105c787U;
        registers[0xcU / 4U] = 0x18d80200U;
        registers[0x10U / 4U] = 0x21965008U;
        registers[0x14U / 4U] = 0x00190002U;
        registers[0x18U / 4U] = 0x00000800U;
        registers[0x1cU / 4U] = 0x00020525U;
        if (isCpuType_133() == 1U)
            registers[0x20U / 4U] = 0x8f000000U;
        registers[0x24U / 4U] = 0x0043000aU;
        registers[0x28U / 4U] = 0x0c633930U;
        if (isCpuType_133() == 1U)
            registers[0x2cU / 4U] = 0x00000aa8U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x05a81140U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x0f00296aU;
        registers[0x4cU / 4U] = 0xa00d2210U;
        registers[0x50U / 4U] = 0x0000000dU;
        registers[0x54U / 4U] = 0x00000000U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200514a8U;
        registers[0x64U / 4U] = 0x00000091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x00003a00U;
        registers[0x70U / 4U] = 0x005fff0fU;
        registers[0x74U / 4U] = 0x00000002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0x00000048U;
        registers[0x80U / 4U] = 0x00000000U;
        registers[0x84U / 4U] = 0x00000400U;
        registers[0x88U / 4U] = 0xffff0000U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x00000100U;
        registers[0x98U / 4U] = 0xa352943fU;
        registers[0x9cU / 4U] = 0x33333333U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x03003333U;
        registers[0xa8U / 4U] = 0x00000818U;
        registers[0xacU / 4U] = 0x0000000dU;
        registers[0xb0U / 4U] = 0x00000000U;
        registers[0xb4U / 4U] = 0x00000000U;
    } else {
        if (isCpuType_133() != 1U)
            return;

        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0xe0000004U;
        registers[0x4U / 4U] = 0x50a840a1U;
        registers[0x8U / 4U] = 0x013e860fU;
        registers[0xcU / 4U] = 0x0210c073U;
        registers[0x10U / 4U] = 0x00000048U;
        registers[0x14U / 4U] = 0x00000000U;
        registers[0x18U / 4U] = 0x00ff8000U;
        registers[0x1cU / 4U] = 0x00238520U;
        registers[0x20U / 4U] = 0x80000200U;
        registers[0x24U / 4U] = 0x00030000U;
        registers[0x28U / 4U] = 0x0c633830U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x05a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x0f002b2aU;
        registers[0x4cU / 4U] = 0x60002200U;
        registers[0x50U / 4U] = 0x34000003U;
        registers[0x54U / 4U] = 0x00000100U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x63f48091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x10cb3f00U;
        registers[0x70U / 4U] = 0xa9404000U;
        registers[0x74U / 4U] = 0x10670002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x103f103fU;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x802dc100U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x55555500U;
        registers[0xa0U / 4U] = 0x55555555U;
        registers[0xa4U / 4U] = 0x00555555U;
        registers[0xa8U / 4U] = 0x30000818U;
        registers[0xacU / 4U] = 0x40002000U;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    }

    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void mode_xgpon_syn_cfg(void)
{
    volatile uint32_t *registers;
    uint8_t product;
    unsigned int product_profile;

    printk("mode_xgpon_syn_cfg\n");
    if (isCpuType_133() != 1U)
        return;

    registers = (volatile uint32_t *)pon_serdes_base;
    registers[0x0U / 4U] = 0xe0000004U;
    registers[0x4U / 4U] = 0x50a840a1U;
    registers[0x8U / 4U] = 0x013e860fU;
    registers[0xcU / 4U] = 0x0210c073U;
    registers[0x10U / 4U] = 0x00000048U;
    registers[0x14U / 4U] = 0x00000000U;
    registers[0x18U / 4U] = 0x00ff8000U;
    registers[0x1cU / 4U] = 0x00238120U;

    product = product_vid;
    product_profile = ((product & 0x7fU) == 0x06U ||
                       (product & 0xfdU) == 0xa4U ||
                       product == 0x63U || product == 0x97U ||
                       (product & 0xfbU) == 0x01U);
    registers[0x20U / 4U] = product_profile ? 0x80000500U : 0x80001000U;
    registers[0x24U / 4U] = 0x00020000U;
    registers[0x28U / 4U] = 0x0c633830U;
    registers[0x2cU / 4U] = 0x00000e46U;
    registers[0x30U / 4U] = 0x00000020U;
    registers[0x34U / 4U] = 0x00002000U;
    registers[0x38U / 4U] = 0x00000000U;
    registers[0x3cU / 4U] = 0x00ff0000U;
    registers[0x40U / 4U] = 0x05a85100U;
    registers[0x44U / 4U] = 0x00000000U;
    registers[0x48U / 4U] = product_profile ? 0x0f002b6aU : 0x0f002b2aU;
    registers[0x4cU / 4U] = 0x60002200U;
    registers[0x50U / 4U] = 0x34000003U;
    registers[0x54U / 4U] = 0x00000100U;
    registers[0x58U / 4U] = 0x00000080U;
    registers[0x5cU / 4U] = 0x00000000U;
    registers[0x60U / 4U] = 0x200554a8U;
    registers[0x64U / 4U] = 0x63f48091U;
    registers[0x68U / 4U] = 0x01000000U;
    registers[0x6cU / 4U] = 0x10cb3f00U;
    registers[0x70U / 4U] = 0xa9404000U;
    registers[0x74U / 4U] = 0x10670002U;
    registers[0x78U / 4U] = 0x000000daU;
    registers[0x7cU / 4U] = 0xf20000c8U;
    registers[0x80U / 4U] = product_profile ? 0x10381038U : 0x103f103fU;
    registers[0x84U / 4U] = 0x01000200U;
    registers[0x88U / 4U] = 0x00020001U;
    registers[0x8cU / 4U] = 0x10000000U;
    registers[0x90U / 4U] = 0x0000001cU;
    registers[0x94U / 4U] = 0x802dc500U;
    registers[0x98U / 4U] = 0x000000ffU;
    registers[0x9cU / 4U] = 0x55555500U;
    registers[0xa0U / 4U] = 0x55555555U;
    registers[0xa4U / 4U] = 0x00555555U;
    registers[0xa8U / 4U] = 0x30000818U;
    registers[0xacU / 4U] = 0x40002000U;
    registers[0xb0U / 4U] = 0x0000000cU;
    registers[0xb4U / 4U] = 0x01000000U;
    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

int eth_an1_clk_set(void)
{
    volatile uint32_t *registers;
    unsigned int retries;

    registers = (volatile uint32_t *)pon_serdes_pll_base;
    registers[0x0U / 4U] = 0x2af8c074U;
    registers[0x4U / 4U] = 0x447424a0U;
    registers[0x8U / 4U] = 0x01050707U;
    registers[0xcU / 4U] = 0x00000107U;
    registers[0x10U / 4U] = 0x00003100U;
    registers[0x14U / 4U] = 0x4000003aU;
    registers[0x18U / 4U] = 0x00000002U;
    registers[0x1cU / 4U] = 0x00130200U;
    registers[0x10U / 4U] |= 1U;

    retries = 1001U;
    while ((registers[0x20U / 4U] & 1U) == 0U) {
        __const_udelay(0x8312b0UL);
        if (--retries == 0U) {
            printk("eth AN1_pll_lock failed!\n");
            return printk("eth AN1_pll_lock_finish\n");
        }
    }

    return printk("eth AN1_pll_lock_finish\n");
}

int an1_pll_epon_cfg(void)
{
    volatile uint32_t *registers;
    unsigned int retries;

    registers = (volatile uint32_t *)pon_serdes_pll_base;
    registers[0x0U / 4U] = 0x2af8c074U;
    registers[0x4U / 4U] = 0x447424a0U;
    registers[0x8U / 4U] = 0x01050707U;
    registers[0xcU / 4U] = 0x00000107U;
    registers[0x10U / 4U] = 0x00003100U;
    registers[0x14U / 4U] = 0x4000003aU;
    registers[0x18U / 4U] = 0x00000002U;
    registers[0x1cU / 4U] = 0x00130200U;
    registers[0x10U / 4U] |= 1U;

    retries = 1001U;
    while ((registers[0x20U / 4U] & 1U) == 0U) {
        __const_udelay(0x418958UL);
        if (--retries == 0U) {
            printk("epon AN1_pll_lock failed!\n");
            return printk("epon AN1_pll_lock_finish\n");
        }
    }

    return printk("epon AN1_pll_lock_finish\n");
}

int an1_pll_gpon_cfg(void)
{
    volatile uint32_t *registers;
    unsigned int retries;

    registers = (volatile uint32_t *)pon_serdes_pll_base;
    registers[0x0U / 4U] = 0x2af8c074U;
    registers[0x4U / 4U] = 0x447424a0U;
    registers[0x8U / 4U] = 0x01050700U;
    registers[0xcU / 4U] = 0x00000107U;
    registers[0x10U / 4U] = 0x00003100U;
    registers[0x14U / 4U] = 0x4000003aU;
    registers[0x18U / 4U] = 0x00000002U;
    registers[0x1cU / 4U] = 0x00130000U;
    registers[0x10U / 4U] |= 1U;

    retries = 1001U;
    while ((registers[0x20U / 4U] & 1U) == 0U) {
        __const_udelay(0x8312b0UL);
        if (--retries == 0U) {
            printk("gpon AN1_pll_lock failed!\n");
            return printk("gpon AN1_pll_lock_finish\n");
        }
    }

    return printk("gpon AN1_pll_lock_finish\n");
}

void mode_eth_10gbase_r_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_eth_10gbase_r_cfg\n");
    if (isCpuType_132() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x2af8c074U;
        registers[0x4U / 4U] = 0x41d124a0U;
        registers[0x8U / 4U] = 0x0145c787U;
        registers[0xcU / 4U] = 0x19000200U;
        registers[0x10U / 4U] = 0x21965008U;
        registers[0x14U / 4U] = 0x00190002U;
        registers[0x18U / 4U] = 0x00000800U;
        registers[0x1cU / 4U] = 0x00020125U;
        registers[0x20U / 4U] = 0x8f001c04U;
        registers[0x24U / 4U] = 0x0042000aU;
        registers[0x28U / 4U] = 0x0c633930U;
        registers[0x2cU / 4U] = 0x00000aa8U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x05a81140U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x8f04296aU;
        registers[0x4cU / 4U] = 0xa00d2210U;
        registers[0x50U / 4U] = 0x0000000dU;
        registers[0x54U / 4U] = 0x00000400U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200514a8U;
        registers[0x64U / 4U] = 0x00000091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x00003a00U;
        registers[0x70U / 4U] = 0x005fff0fU;
        registers[0x74U / 4U] = 0x00000002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0x00000048U;
        registers[0x80U / 4U] = 0x00000000U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x00000000U;
        registers[0x98U / 4U] = 0xa352943fU;
        registers[0x9cU / 4U] = 0x33333333U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x03003333U;
        registers[0xa8U / 4U] = 0x00000818U;
        registers[0xacU / 4U] = 0x0000000dU;
        registers[0xb0U / 4U] = 0x00000000U;
        registers[0xb4U / 4U] = 0x00000000U;
    } else {
        if (isCpuType_133() != 1U)
            return;

        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0xe0000004U;
        registers[0x4U / 4U] = 0x50a840a3U;
        registers[0x8U / 4U] = 0x013e8687U;
        registers[0xcU / 4U] = 0x0210c073U;
        registers[0x10U / 4U] = 0x00000048U;
        registers[0x14U / 4U] = 0x00000000U;
        registers[0x18U / 4U] = 0x00ff8000U;
        registers[0x1cU / 4U] = 0x00238020U;
        registers[0x20U / 4U] = 0x80000400U;
        registers[0x24U / 4U] = 0x00020000U;
        registers[0x28U / 4U] = 0x0c633830U;
        registers[0x2cU / 4U] = 0x00000e46U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x05a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x4f042b2aU;
        registers[0x4cU / 4U] = 0x60002200U;
        registers[0x50U / 4U] = 0x34000003U;
        registers[0x54U / 4U] = 0x00000100U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x67748091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x40003b00U;
        registers[0x70U / 4U] = 0xa9004000U;
        registers[0x74U / 4U] = 0x10670002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x10371037U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x00084008U;
        registers[0x94U / 4U] = 0x802dc000U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x55555500U;
        registers[0xa0U / 4U] = 0x55555555U;
        registers[0xa4U / 4U] = 0x00555555U;
        registers[0xa8U / 4U] = 0x30000818U;
        registers[0xacU / 4U] = 0x40002000U;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    }

    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void mode_eth_5gbase_r_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_eth_5gbase_r_cfg\n");
    if (isCpuType_132() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x2af8c074U;
        registers[0x4U / 4U] = 0x41d124a0U;
        registers[0x8U / 4U] = 0x0145c787U;
        registers[0xcU / 4U] = 0x19000200U;
        registers[0x10U / 4U] = 0x21965008U;
        registers[0x14U / 4U] = 0x00190002U;
        registers[0x18U / 4U] = 0x00000800U;
        registers[0x1cU / 4U] = 0x00020125U;
        registers[0x20U / 4U] = 0x8f000400U;
        registers[0x24U / 4U] = 0x0042000aU;
        registers[0x28U / 4U] = 0x0c633930U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x05a81140U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x8f04296aU;
        registers[0x4cU / 4U] = 0xa20d2210U;
        registers[0x50U / 4U] = 0x0000000dU;
        registers[0x54U / 4U] = 0x00000400U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200514a8U;
        registers[0x64U / 4U] = 0x00000091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x00003a00U;
        registers[0x70U / 4U] = 0x005fff0fU;
        registers[0x74U / 4U] = 0x00000002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0x00000048U;
        registers[0x80U / 4U] = 0x00000000U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0008001cU;
        registers[0x94U / 4U] = 0x00000000U;
        registers[0x98U / 4U] = 0xa352943fU;
        registers[0x9cU / 4U] = 0x33333333U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x03003333U;
        registers[0xa8U / 4U] = 0x00000818U;
        registers[0xacU / 4U] = 0x0000000dU;
        registers[0xb0U / 4U] = 0x00000000U;
        registers[0xb4U / 4U] = 0x00000000U;
    } else {
        if (isCpuType_133() != 1U)
            return;

        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0xe0000004U;
        registers[0x4U / 4U] = 0x50a840a3U;
        registers[0x8U / 4U] = 0x013e864fU;
        registers[0xcU / 4U] = 0x0210c073U;
        registers[0x10U / 4U] = 0x00000048U;
        registers[0x14U / 4U] = 0x00000000U;
        registers[0x18U / 4U] = 0x00ff8000U;
        registers[0x1cU / 4U] = 0x00238220U;
        registers[0x20U / 4U] = 0x80000200U;
        registers[0x24U / 4U] = 0x00020000U;
        registers[0x28U / 4U] = 0x0c633830U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x45a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x4f002b2aU;
        registers[0x4cU / 4U] = 0x62002220U;
        registers[0x50U / 4U] = 0x34000003U;
        registers[0x54U / 4U] = 0x00000100U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200574a8U;
        registers[0x64U / 4U] = 0x67748091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x40003b00U;
        registers[0x70U / 4U] = 0xa9004000U;
        registers[0x74U / 4U] = 0x10670002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x10371037U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x00084008U;
        registers[0x94U / 4U] = 0x802dc000U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x55555500U;
        registers[0xa0U / 4U] = 0x55555555U;
        registers[0xa4U / 4U] = 0x00555555U;
        registers[0xa8U / 4U] = 0x30000818U;
        registers[0xacU / 4U] = 0x40002000U;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    }

    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void mode_eth_2p5gbase_r_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_eth_2p5gbase_r_cfg\n");
    if (isCpuType_132() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x2af8c074U;
        registers[0x4U / 4U] = 0x41d124a0U;
        registers[0x8U / 4U] = 0x0145c787U;
        registers[0xcU / 4U] = 0x19000200U;
        registers[0x10U / 4U] = 0x21965008U;
        registers[0x14U / 4U] = 0x00190002U;
        registers[0x18U / 4U] = 0x00000800U;
        registers[0x1cU / 4U] = 0x00020125U;
        registers[0x20U / 4U] = 0x8f000000U;
        registers[0x24U / 4U] = 0x0042000aU;
        registers[0x28U / 4U] = 0x0c633930U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x03a81140U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x8f04296aU;
        registers[0x4cU / 4U] = 0x24092210U;
        registers[0x50U / 4U] = 0x00000009U;
        registers[0x54U / 4U] = 0x00000400U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200514a8U;
        registers[0x64U / 4U] = 0x00000091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x00003a00U;
        registers[0x70U / 4U] = 0x005fff0fU;
        registers[0x74U / 4U] = 0x00000002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0x00000048U;
        registers[0x80U / 4U] = 0x00000000U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0010001cU;
        registers[0x94U / 4U] = 0x00000000U;
        registers[0x98U / 4U] = 0xa352943fU;
        registers[0x9cU / 4U] = 0x33333333U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x03003333U;
        registers[0xa8U / 4U] = 0x00000818U;
        registers[0xacU / 4U] = 0x0000000dU;
        registers[0xb0U / 4U] = 0x00000000U;
        registers[0xb4U / 4U] = 0x00000000U;
    } else {
        if (isCpuType_133() != 1U && isCpuType_129() != 1U)
            return;

        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0xe0000004U;
        registers[0x4U / 4U] = 0x50a840a3U;
        registers[0x8U / 4U] = 0x013e8620U;
        registers[0xcU / 4U] = 0x0210c073U;
        registers[0x10U / 4U] = 0x00000048U;
        registers[0x14U / 4U] = 0x00000000U;
        registers[0x18U / 4U] = 0x00ff8000U;
        registers[0x1cU / 4U] = 0x00238420U;
        registers[0x20U / 4U] = 0x80000100U;
        registers[0x24U / 4U] = 0x00020000U;
        registers[0x28U / 4U] = 0x0c633830U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x85a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x4f002b2aU;
        registers[0x4cU / 4U] = 0x64002220U;
        registers[0x50U / 4U] = 0x34000003U;
        registers[0x54U / 4U] = 0x00000100U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200574a8U;
        registers[0x64U / 4U] = 0x67748091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x40003b00U;
        registers[0x70U / 4U] = 0xa9004000U;
        registers[0x74U / 4U] = 0x10670002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x10371037U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x00084008U;
        registers[0x94U / 4U] = 0x802dc000U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x55555500U;
        registers[0xa0U / 4U] = 0x55555555U;
        registers[0xa4U / 4U] = 0x00555555U;
        registers[0xa8U / 4U] = 0x30000818U;
        registers[0xacU / 4U] = 0x40002000U;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    }

    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void mode_eth_2p5gbase_x_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_eth_2p5gbase_x_cfg\n");
    if (isCpuType_132() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x2af8c877U;
        registers[0x4U / 4U] = 0x41d124a0U;
        registers[0x8U / 4U] = 0x0145c787U;
        registers[0xcU / 4U] = 0x05080a00U;
        registers[0x10U / 4U] = 0x00000008U;
        registers[0x14U / 4U] = 0x00194000U;
        registers[0x18U / 4U] = 0x00000800U;
        registers[0x1cU / 4U] = 0x00020225U;
        registers[0x20U / 4U] = 0x8f000000U;
        registers[0x24U / 4U] = 0x0045000aU;
        registers[0x28U / 4U] = 0x0c633930U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x03a89140U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x040458eaU;
        registers[0x4cU / 4U] = 0x20292230U;
        registers[0x50U / 4U] = 0x00000029U;
        registers[0x54U / 4U] = 0x00000408U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200574a8U;
        registers[0x64U / 4U] = 0x00001092U;
        registers[0x68U / 4U] = 0x01400600U;
        registers[0x6cU / 4U] = 0x00002a00U;
        registers[0x70U / 4U] = 0x005fff0fU;
        registers[0x74U / 4U] = 0x00002002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0x00000048U;
        registers[0x80U / 4U] = 0x00000000U;
        registers[0x84U / 4U] = 0x00000400U;
        registers[0x88U / 4U] = 0xffff0000U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x00000000U;
        registers[0x98U / 4U] = 0xa352943fU;
        registers[0x9cU / 4U] = 0x33333333U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x03003333U;
        registers[0xa8U / 4U] = 0x00000818U;
        registers[0xacU / 4U] = 0x0000000dU;
        registers[0xb0U / 4U] = 0x00000000U;
        registers[0xb4U / 4U] = 0x00000000U;
    } else if (isCpuType_133() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0xe0000004U;
        registers[0x4U / 4U] = 0x4fa8c0a2U;
        registers[0x8U / 4U] = 0x013e8604U;
        registers[0xcU / 4U] = 0x1f51c8f3U;
        registers[0x10U / 4U] = 0x00000044U;
        registers[0x14U / 4U] = 0x00194000U;
        registers[0x18U / 4U] = 0x0b080000U;
        registers[0x1cU / 4U] = 0x00238220U;
        registers[0x20U / 4U] = 0x80000100U;
        registers[0x24U / 4U] = 0x00050000U;
        registers[0x28U / 4U] = 0x0c633830U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x05a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x04005b6aU;
        registers[0x4cU / 4U] = 0x60002220U;
        registers[0x50U / 4U] = 0x34000003U;
        registers[0x54U / 4U] = 0x00000408U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200574a8U;
        registers[0x64U / 4U] = 0x00649052U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x40007700U;
        registers[0x70U / 4U] = 0xa9004000U;
        registers[0x74U / 4U] = 0x01108002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x10371037U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x802dc000U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x55555500U;
        registers[0xa0U / 4U] = 0x55555555U;
        registers[0xa4U / 4U] = 0x00555555U;
        registers[0xa8U / 4U] = 0x30000818U;
        registers[0xacU / 4U] = 0x40002000U;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    } else {
        if (isCpuType_129() != 1U)
            return;

        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x00070004U;
        registers[0x4U / 4U] = 0x000040a0U;
        registers[0x8U / 4U] = 0x00008009U;
        registers[0xcU / 4U] = 0x1f500000U;
        registers[0x10U / 4U] = 0x00000004U;
        registers[0x14U / 4U] = 0x00194000U;
        registers[0x18U / 4U] = 0x00000000U;
        registers[0x1cU / 4U] = 0x00000220U;
        registers[0x20U / 4U] = 0x80000200U;
        registers[0x24U / 4U] = 0x00000000U;
        registers[0x28U / 4U] = 0x00000010U;
        registers[0x2cU / 4U] = 0x00000421U;
        registers[0x30U / 4U] = 0x00000000U;
        registers[0x34U / 4U] = 0x00000000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00000000U;
        registers[0x40U / 4U] = 0x05a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x04040b6aU;
        registers[0x4cU / 4U] = 0x80002200U;
        registers[0x50U / 4U] = 0x00000604U;
        registers[0x54U / 4U] = 0x00000400U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x3de49092U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0xc0003700U;
        registers[0x70U / 4U] = 0xa9004000U;
        registers[0x74U / 4U] = 0x10108000U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x10371037U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x00004000U;
        registers[0x94U / 4U] = 0x00000000U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x33333300U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x00333333U;
        registers[0xa8U / 4U] = 0x20000818U;
        registers[0xacU / 4U] = 0x0000201cU;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    }

    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void mode_eth_1gbase_x_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_eth_1gbase_x_cfg\n");
    if (isCpuType_132() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x2af8c874U;
        registers[0x4U / 4U] = 0x41d124a0U;
        registers[0x8U / 4U] = 0x0105c787U;
        registers[0xcU / 4U] = 0x19000200U;
        registers[0x10U / 4U] = 0x21965008U;
        registers[0x14U / 4U] = 0x00190002U;
        registers[0x18U / 4U] = 0x00000800U;
        registers[0x1cU / 4U] = 0x00020125U;
        registers[0x20U / 4U] = 0x8f000000U;
        registers[0x24U / 4U] = 0x0045000aU;
        registers[0x28U / 4U] = 0x0c633930U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x03a81140U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x0f04596aU;
        registers[0x4cU / 4U] = 0x26292210U;
        registers[0x50U / 4U] = 0x00000029U;
        registers[0x54U / 4U] = 0x00000400U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200514a8U;
        registers[0x64U / 4U] = 0x00000091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x00003a00U;
        registers[0x70U / 4U] = 0x005fff0fU;
        registers[0x74U / 4U] = 0x00000002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0x00000048U;
        registers[0x80U / 4U] = 0x00000000U;
        registers[0x84U / 4U] = 0x00000400U;
        registers[0x88U / 4U] = 0xffff0000U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0018001cU;
        registers[0x94U / 4U] = 0x00000000U;
        registers[0x98U / 4U] = 0xa352943fU;
        registers[0x9cU / 4U] = 0x33333333U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x03003333U;
        registers[0xa8U / 4U] = 0x00000818U;
        registers[0xacU / 4U] = 0x0000000dU;
        registers[0xb0U / 4U] = 0x00000000U;
        registers[0xb4U / 4U] = 0x00000000U;
    } else if (isCpuType_133() == 1U) {
        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0xe0000004U;
        registers[0x4U / 4U] = 0x4fa840a3U;
        registers[0x8U / 4U] = 0x013e860fU;
        registers[0xcU / 4U] = 0x0210c073U;
        registers[0x10U / 4U] = 0x00000048U;
        registers[0x14U / 4U] = 0x00000000U;
        registers[0x18U / 4U] = 0x00ff8000U;
        registers[0x1cU / 4U] = 0x00238620U;
        registers[0x20U / 4U] = 0x80000000U;
        registers[0x24U / 4U] = 0x00050000U;
        registers[0x28U / 4U] = 0x0c633830U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0xc5a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x01045b2aU;
        registers[0x4cU / 4U] = 0x66002200U;
        registers[0x50U / 4U] = 0x34000003U;
        registers[0x54U / 4U] = 0x00000100U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x00648091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x00003700U;
        registers[0x70U / 4U] = 0xa9004000U;
        registers[0x74U / 4U] = 0x10640002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x10371037U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x8000c000U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x55555500U;
        registers[0xa0U / 4U] = 0x55555555U;
        registers[0xa4U / 4U] = 0x00555555U;
        registers[0xa8U / 4U] = 0x30000818U;
        registers[0xacU / 4U] = 0x40002000U;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    } else {
        if (isCpuType_129() != 1U)
            return;

        registers = (volatile uint32_t *)pon_serdes_base;
        registers[0x0U / 4U] = 0x00070004U;
        registers[0x4U / 4U] = 0x000040a0U;
        registers[0x8U / 4U] = 0x00008003U;
        registers[0xcU / 4U] = 0x0c900000U;
        registers[0x10U / 4U] = 0x00000004U;
        registers[0x14U / 4U] = 0x00150000U;
        registers[0x18U / 4U] = 0x00000000U;
        registers[0x1cU / 4U] = 0x00000220U;
        registers[0x20U / 4U] = 0x80000200U;
        registers[0x24U / 4U] = 0x00000000U;
        registers[0x28U / 4U] = 0x00000010U;
        registers[0x2cU / 4U] = 0x00000421U;
        registers[0x30U / 4U] = 0x00000000U;
        registers[0x34U / 4U] = 0x00000000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00000000U;
        registers[0x40U / 4U] = 0x05a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x01040b6aU;
        registers[0x4cU / 4U] = 0x60002200U;
        registers[0x50U / 4U] = 0x00000603U;
        registers[0x54U / 4U] = 0x00000400U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x3de49092U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0xc0003700U;
        registers[0x70U / 4U] = 0xa9004000U;
        registers[0x74U / 4U] = 0x10108000U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x10371037U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x00004000U;
        registers[0x94U / 4U] = 0x00000000U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x33333300U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x00333333U;
        registers[0xa8U / 4U] = 0x20000818U;
        registers[0xacU / 4U] = 0x0000201cU;
        registers[0xb0U / 4U] = 0x0000000cU;
        registers[0xb4U / 4U] = 0x01000000U;
    }

    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

int an1_pll_clk_set(uint32_t mode)
{
    uint64_t mode_bit;

    if (mode > 16U)
        return (int)mode;

    mode_bit = UINT64_C(1) << mode;
    if ((mode_bit & UINT64_C(0x1ff00)) != 0U)
        return eth_an1_clk_set();
    if ((mode_bit & UINT64_C(0x0e0)) != 0U)
        return an1_pll_gpon_cfg();
    if ((mode_bit & UINT64_C(0x01f)) != 0U)
        return an1_pll_epon_cfg();

    return (int)mode_bit;
}

void serdes_mode_set(uint32_t mode)
{
    switch (mode) {
    case 0U:
        mode_epon_cfg();
        break;
    case 1U:
        mode_10g_epon_nsyn_dpll_cfg();
        break;
    case 2U:
        mode_10g_epon_nsyn_fifo_cfg();
        break;
    case 3U:
        mode_10g_epon_nsyn_nofifo_cfg();
        break;
    case 4U:
        mode_10g_epon_syn_cfg();
        break;
    case 5U:
        mode_gpon_cfg();
        break;
    case 6U:
        mode_xgpon_nsyn_cfg();
        break;
    case 7U:
        mode_xgpon_syn_cfg();
        break;
    case 8U:
    case 16U:
        mode_eth_1gbase_x_cfg();
        break;
    case 9U:
    case 15U:
        mode_eth_2p5gbase_x_cfg();
        break;
    case 10U:
        mode_eth_2p5gbase_r_cfg();
        break;
    case 11U:
    case 12U:
        mode_eth_5gbase_r_cfg();
        break;
    case 13U:
    case 14U:
        mode_eth_10gbase_r_cfg();
        break;
    default:
        return;
    }
}

int pon_serdes_init(uint32_t mode)
{
    volatile uint32_t *registers;
    unsigned int retries;

    if (isCpuType_132() == 1U)
        an1_pll_clk_set(mode);
    serdes_mode_set(mode);

    registers = (volatile uint32_t *)pon_serdes_base;
    registers[0x90U / 4U] = (registers[0x90U / 4U] & 0xffff9fffU) | 0x4000U;
    registers[0x40U / 4U] |= 0x8000U;
    registers[0x54U / 4U] |= 1U;

    if (isCpuType_132() == 1U || isCpuType_133() == 1U) {
        retries = 1001U;
        while ((registers[0xd0U / 4U] & 1U) == 0U) {
            __const_udelay(0x8312b0UL);
            if (--retries == 0U) {
                printk("com pll lock failed!\n");
                return -1;
            }
        }
        printk("com_pll_lock_ready\n");
    } else if (isCpuType_129() == 1U) {
        retries = 1001U;
        while ((registers[0xccU / 4U] & 2U) == 0U) {
            __const_udelay(0x8312b0UL);
            if (--retries == 0U) {
                printk("com pll lock failed!\n");
                return -1;
            }
        }
        printk("com_pll_lock_ready\n");
    }

    if ((registers[0xe4U / 4U] & 1U) != 0U)
        printk("rx los =1 no  rx data in\n");
    else
        printk("rx los =0 rx data in\n");

    retries = 1001U;
    while ((registers[0xe4U / 4U] & 2U) == 0U) {
        __const_udelay(0x8312b0UL);
        if (--retries == 0U) {
            printk("cdr lock failed!\n");
            return -1;
        }
    }
    printk("cdr_lock_ready\n");

    if (isCpuType_129() == 1U) {
        retries = 1001U;
        while ((registers[0xe4U / 4U] & 0x200U) == 0U &&
               (registers[0xe4U / 4U] & 0x400U) == 0U) {
            __const_udelay(0x8312b0UL);
            if (--retries == 0U) {
                printk("mux_txpcs_rst_n or mux_rxpcs_rst_n failed!\n");
                return -1;
            }
        }
        printk("serdes clock send out\n");
    }

    return 0;
}

int pon_pll_cfg(uint32_t mode)
{
    volatile uint32_t *registers;

    if (mode <= 4U) {
        printk("enter epon pon pll cfg \n");
        registers = (volatile uint32_t *)top_crm_base;
        registers[0x10U / 4U] =
            (registers[0x10U / 4U] & 0xffffffcfU) | 0x20U;
        if (isCpuType_132() == 1U) {
            registers[0xc0U / 4U] = 0x00202855U;
            registers[0xc4U / 4U] = 0x0a000000U;
            registers[0xc4U / 4U] |= 0x10000000U;
        } else {
            registers[0xc4U / 4U] |= 0x10000000U;
            registers[0xc0U / 4U] = 0x20101054U;
            registers[0xc4U / 4U] = 0x04000000U;
        }
    } else if (mode >= 5U && mode <= 7U) {
        printk("enter gpon pon pll cfg %px\n", (const void *)top_crm_base);
        registers = (volatile uint32_t *)top_crm_base;
        registers[0x10U / 4U] =
            (registers[0x10U / 4U] & 0xffffffcfU) | 0x20U;
        if (isCpuType_132() == 1U) {
            registers[0xc0U / 4U] = 0x20202054U;
            registers[0xc4U / 4U] = 0x0a2673e3U;
            registers[0xc4U / 4U] |= 0x10000000U;
        } else {
            registers[0xc4U / 4U] |= 0x10000000U;
            registers[0xc0U / 4U] = 0x00202054U;
            registers[0xc4U / 4U] = 0x042673e2U;
        }
    } else if (mode >= 8U && mode <= 16U) {
        printk("enter eth pll cfg \n");
        registers = (volatile uint32_t *)top_crm_base;
        registers[0x10U / 4U] &= 0xffffffcfU;
        registers[0xc4U / 4U] |= 0x10000000U;
        registers[0xc0U / 4U] = 0x20106454U;
        registers[0xc4U / 4U] = 0x04000000U;
    } else {
        return 0;
    }

    registers[0xcU / 4U] |= 0x200U;
    registers[0xcU / 4U] &= ~0x100U;
    return 0;
}

void zx_pon_clk_reset_init(uint32_t mode)
{
    volatile uint32_t *registers;
    unsigned int delay_count;

    pon_pll_cfg(mode);
    pon_serdes_mode = mode;

    if (isCpuType_129() == 1U) {
        registers = (volatile uint32_t *)top_crm_base;
        registers[0x70U / 4U] &= ~1U;
        registers[0x70U / 4U] &= ~2U;
        delay_count = 11U;
        while (--delay_count != 0U)
            __const_udelay(0x418958UL);

        registers[0x70U / 4U] |= 1U;
        registers[0x70U / 4U] |= 2U;
        delay_count = 11U;
        while (--delay_count != 0U)
            __const_udelay(0x418958UL);
    } else if (isCpuType_132() == 1U) {
        registers = (volatile uint32_t *)top_crm_base;
        registers[0x70U / 4U] &= ~1U;
        registers[0x70U / 4U] &= ~2U;
        registers[0x60U / 4U] &= ~0x200U;
        delay_count = 11U;
        while (--delay_count != 0U)
            __const_udelay(0x418958UL);

        registers[0x70U / 4U] |= 1U;
        registers[0x60U / 4U] |= 0x200U;
        registers[0x70U / 4U] |= 2U;
        delay_count = 11U;
        while (--delay_count != 0U)
            __const_udelay(0x418958UL);
    } else if (isCpuType_133() == 1U) {
        registers = (volatile uint32_t *)top_crm_base;
        registers[0x70U / 4U] &= ~1U;
        registers[0x70U / 4U] &= ~2U;
        delay_count = 11U;
        while (--delay_count != 0U)
            __const_udelay(0x418958UL);

        registers[0x70U / 4U] |= 1U;
        registers[0x70U / 4U] |= 2U;
        delay_count = 11U;
        while (--delay_count != 0U)
            __const_udelay(0x418958UL);
    }

    if (pon_serdes_init(mode) != 0)
        printk("serdes init failed because the pll or cdr is not lock\n");
    else
        printk("pon serdes init succeed\n");
}

volatile uint32_t *uni_apb_write(volatile uint32_t *address, uint32_t value)
{
    *address = value;
    return address;
}

uint32_t uni_apb_read(const volatile uint32_t *address)
{
    return *address;
}

volatile uint32_t *uni_apb_bit_write(volatile uint32_t *address,
                                     uint32_t value,
                                     uint32_t width,
                                     uint32_t shift)
{
    uint32_t mask;

    mask = ((1U << width) - 1U) << shift;
    *address = (*address & ~mask) | (value << shift);
    return address;
}

int uni_serdes_err_cnt_reset(void)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x94U / 4U] &= ~0x8000U;
    registers[0x94U / 4U] |= 0x8000U;
    return 0;
}

uint32_t uni_serdes_set_pattern(uint32_t pattern_low,
                                uint32_t pattern_high,
                                uint16_t control_low,
                                int enable)
{
    volatile uint32_t *registers;
    uint32_t control;

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x94U / 4U] &= 0xffff0fffU;
    registers[0x9cU / 4U] = pattern_low;
    registers[0xa0U / 4U] = pattern_high;
    registers[0xa4U / 4U] = (registers[0xa4U / 4U] & 0xffff0000U) | control_low;
    control = registers[0xa4U / 4U];
    if (enable == 1)
        control |= 0x00070000U;
    else
        control &= 0xfff8ffffU;
    registers[0xa4U / 4U] = control;
    return control;
}

int zx_uni_clk_reset_init(void)
{
    return 0;
}

int uni_com_pll_cfg_ring_circle_bisa_set(uint32_t value)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x4U / 4U] = (registers[0x4U / 4U] & 0xfff0ffffU) | (value << 16);
    printk("default:0100 100U           0000:0 0001:25u 0010:50u 0011:75u 0100:100u 0101:125u 0110:150u 0111:175u           1000:200u 1001:225u 1010:250u 1011:275u 1100:300u 1101:325u 1110:350u 1111:375u \n");
    return printk("com pll ring circle I set data=0x%x\n", value);
}

uint32_t uni_com_pll_cfg_ring_circle_bisa_get(void)
{
    volatile uint32_t *registers;
    uint32_t value;

    registers = (volatile uint32_t *)uni_serdes_base;
    value = (registers[0x4U / 4U] >> 16) & 0x0fU;
    printk("an1_pll_cfg_ring_circle_bisa is 0x%x\n", value);
    printk("default:0100 100U           0000:0 0001:25u 0010:50u 0011:75u 0100:100u 0101:125u 0110:150u 0111:175u           1000:200u 1001:225u 1010:250u 1011:275u 1100:300u 1101:325u 1110:350u 1111:375u \n");
    return value;
}

int uni_com_pll_cfg_ring_circle_resl_set(uint32_t value)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x4U / 4U] = (registers[0x4U / 4U] & 0xf87fffffU) | (value << 23);
    return printk("com pll ring circle R set data=0x%x\n", value);
}

uint32_t uni_com_pll_cfg_ring_circle_resl_get(void)
{
    volatile uint32_t *registers;
    uint32_t value;

    registers = (volatile uint32_t *)uni_serdes_base;
    value = (registers[0x4U / 4U] >> 23) & 0x0fU;
    printk("an1_pll_cfg_ring_circle_resl is 0x%x\n", value);
    return value;
}

int uni_serdes_set_tx_swin(uint32_t value)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x20U / 4U] = (registers[0x20U / 4U] & 0xfffcffffU) | (value << 16);
    return printk("set the tx swin is 0x%x\n", value);
}

int uni_serdes_set_low_power(uint32_t mode)
{
    volatile uint32_t *registers;

    if (mode > 5U)
        return printk("LOW POWER MODE IS ERROR\n");

    registers = (volatile uint32_t *)uni_serdes_base;
    switch (mode) {
    case 0U:
        registers[0x5cU / 4U] &= 0xffffff00U;
        return printk("enter normal mode\n");
    case 1U:
        registers[0x5cU / 4U] |= 0xffU;
        return printk("enter low power mode\n");
    case 2U:
        registers[0x5cU / 4U] = (registers[0x5cU / 4U] & 0xffffff00U) | 0xddU;
        return printk("enter sleep mode\n");
    case 3U:
        registers[0x5cU / 4U] = (registers[0x5cU / 4U] & 0xffffff00U) | 0x22U;
        return printk("enter small flow mode\n");
    case 4U:
        registers[0x5cU / 4U] = (registers[0x5cU / 4U] & 0xffffff00U) | 0x33U;
        return printk("enter rx en and tx off mode\n");
    default:
        return printk("the low power mode is error\n");
    }
}

int uni_serdes_set_band(uint32_t band_select, uint32_t band_value)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x6cU / 4U] =
        (registers[0x6cU / 4U] & 0xffffbfffU) | (band_select << 14);
    registers[0x6cU / 4U] =
        (registers[0x6cU / 4U] & 0xffffff00U) | band_value;
    return printk("set pll band ok\n");
}

uint32_t uni_serdes_get_band(void)
{
    volatile uint32_t *registers;
    uint32_t value;

    registers = (volatile uint32_t *)uni_serdes_base;
    value = (registers[0xd0U / 4U] >> 16) & 0xffU;
    printk("uni_serdes_get_band is 0x%x\n", value);
    return value;
}

int uni_serdes_set_gen_en(uint32_t value)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x94U / 4U] = (registers[0x94U / 4U] & 0xffffdfffU) | (value << 13);
    return printk("set the prbs gen en=0x%x\n", value);
}

int uni_serdes_set_check_en(uint32_t value)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x94U / 4U] = (registers[0x94U / 4U] & 0xffffbfffU) | (value << 14);
    return printk("set check en =0x%x\n", value);
}

int uni_serdes_set_err_cnt_en(uint32_t value)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x94U / 4U] = (registers[0x94U / 4U] & 0xffff7fffU) | (value << 15);
    return printk("set prbs error cnt  en =0x%x\n", value);
}

uint64_t uni_serdes_get_err_cnt(void)
{
    volatile uint32_t *registers;
    uint64_t value;

    registers = (volatile uint32_t *)uni_serdes_base;
    value = registers[0xe8U / 4U];
    value |= (uint64_t)(registers[0xecU / 4U] & 0xffffU) << 32;
    printk("uni_serdes_get_err_cnt =0x%lx\n", value);
    return value;
}

int uni_serdes_prbs_err_ok(void)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x48U / 4U] |= 0x00800000U;
    return printk("set 1 bit error ok\n");
}

int uni_serdes_set_error_time_en(uint32_t value)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)pon_serdes_base;
    registers[0x94U / 4U] = (registers[0x94U / 4U] & 0xbfffffffU) | (value << 30);
    return printk("set error time is ok\n");
}

int uni_serdes_set_error_time(uint32_t time_units)
{
    volatile uint32_t *registers;
    uint32_t ticks;
    uint32_t mode;
    uint64_t reported_ticks;

    mode = uni_serdes_mode;
    ticks = 0U;
    if (mode <= 4U)
        ticks = 156250000U * time_units;
    if ((mode >= 5U && mode <= 7U) || mode == 17U)
        ticks = 155520000U * time_units;

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x98U / 4U] = ticks;
    registers[0xa4U / 4U] &= 0x00ffffffU;
    reported_ticks = registers[0x98U / 4U];
    reported_ticks |= (uint64_t)((registers[0xa4U / 4U] >> 24) & 0xffU) << 32;
    return printk("uni_serdes_set_error_time_en : %ld.\n", reported_ticks);
}

int uni_serdesPrbsCounterGetHandler(void)
{
    uint64_t error_count;

    error_count = uni_serdes_get_err_cnt();
    if (error_count < uni_serdesPrbsCounter)
        return printk("pon serdes ztePonGetPrbsCounters Error: Overflow detected\n");

    return printk("pon serdes ztePonGetPrbsCounters counter: %ld.\n",
                  error_count - uni_serdesPrbsCounter);
}

int uni_serdes_set_rx_eq_mbf(uint32_t value)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x2cU / 4U] = (registers[0x2cU / 4U] & 0xffc3ffffU) | (value << 18);
    return printk("set serdes rx eq mbf 0x%x\n", value);
}

int uni_serdes_get_rx_eq(void)
{
    volatile uint32_t *registers;
    uint32_t register_value;

    registers = (volatile uint32_t *)uni_serdes_base;
    register_value = registers[0x2cU / 4U];
    if ((register_value & 1U) != 0U) {
        printk("serdes rx eq1 is disable");
    } else {
        printk("serdes rx eq1 is enable");
        printk("serdes rx eq1 data is 0x%x\n", (register_value >> 3) & 0x1fU);
    }

    if ((register_value & 2U) != 0U) {
        printk("serdes rx eq2 is disable");
    } else {
        printk("serdes rx eq2 is enable");
        printk("serdes rx eq2 data is 0x%x\n", (register_value >> 8) & 0x1fU);
    }

    if ((register_value & 4U) != 0U) {
        printk("serdes rx eq3 is disable");
    } else {
        printk("serdes rx eq3 is enable");
        printk("serdes rx eq3 data is 0x%x\n", (register_value >> 13) & 0x1fU);
    }

    return printk("serdes rx eq mbf is 0x%x\n", (register_value >> 18) & 0x0fU);
}

int uni_serdes_set_np_jittery(uint32_t value)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x48U / 4U] = (registers[0x48U / 4U] & 0xfffffe3fU) | (value << 6);
    return printk("serdes_np_jittery_set 0x%x\n", value);
}

uint32_t uni_serdes_get_np_jittery(void)
{
    volatile uint32_t *registers;
    uint32_t value;

    registers = (volatile uint32_t *)uni_serdes_base;
    value = (registers[0x48U / 4U] >> 6) & 0x07U;
    printk("serdes_np_jittery_get 0x%x\n", value);
    return value;
}

int pin_mux_debug_clk_133_out0(uint32_t pin_mux_value,
                               uint32_t low_field,
                               uint32_t field_16,
                               uint32_t field_20)
{
    volatile uint32_t *debug_registers;
    uint32_t pin_mux_state;
    uint32_t logged_field_16;
    uint32_t logged_field_20;
    uint32_t logged_low_field;

    debug_registers = (volatile uint32_t *)top_crm_base;
    debug_registers[0x1d0U / 4U] |= 0x10U;
    pin_mux_base[0] = (pin_mux_base[0] & 0xfffffffcU) | pin_mux_value;
    debug_registers[0x1d0U / 4U] =
        (debug_registers[0x1d0U / 4U] & 0xfff8ffffU) | (field_16 << 16);
    debug_registers[0x1d0U / 4U] =
        (debug_registers[0x1d0U / 4U] & 0xffcfffffU) | (field_20 << 20);
    debug_registers[0x1d0U / 4U] =
        (debug_registers[0x1d0U / 4U] & 0xfffffff0U) | low_field;

    pin_mux_state = pin_mux_base[0];
    printk("pin mux teset clk0=0x%x \n", (pin_mux_state >> 1) & 0x07U);
    logged_field_16 = (debug_registers[0x1d0U / 4U] >> 16) & 0x07U;
    logged_field_20 = (debug_registers[0x1d0U / 4U] >> 20) & 0x03U;
    logged_low_field = debug_registers[0x1d0U / 4U] & 0x0fU;
    return printk(
        "top crm debug clk(0x10e101d0) bit[18:16]=0x%x  bit[21:20]=0x%x bit[3:0]=0x%x\n",
        logged_field_16, logged_field_20, logged_low_field);
}

int pin_mux_debug_clk_133_out1(uint32_t pin_mux_value,
                               uint32_t field_8,
                               uint32_t field_24)
{
    volatile uint32_t *debug_registers;
    uint32_t pin_mux_state;
    uint32_t logged_field_24;
    uint32_t logged_field_8;

    debug_registers = (volatile uint32_t *)top_crm_base;
    debug_registers[0x1d0U / 4U] |= 0x1000U;
    pin_mux_base[0x4U / 4U] =
        (pin_mux_base[0x4U / 4U] & 0xffff8fffU) | (pin_mux_value << 12);
    debug_registers[0x1d0U / 4U] =
        (debug_registers[0x1d0U / 4U] & 0xf8ffffffU) | (field_24 << 24);
    debug_registers[0x1d0U / 4U] =
        (debug_registers[0x1d0U / 4U] & 0xfffff0ffU) | (field_8 << 8);

    pin_mux_state = pin_mux_base[0x4U / 4U];
    printk("pin mux teset clk1=0x%x \n", (pin_mux_state >> 15) & 0x07U);
    logged_field_24 = (debug_registers[0x1d0U / 4U] >> 24) & 0x07U;
    logged_field_8 = (debug_registers[0x1d0U / 4U] >> 8) & 0x0fU;
    return printk(
        "top crm debug clk(0x10e101d0)  bit[26:24]=0x%x bit[11:8]=0x%x\n",
        logged_field_24, logged_field_8);
}

int uni_check_serdes_config(void)
{
    volatile uint32_t *registers;
    uint32_t offset;

    switch (uni_serdes_mode) {
    case 0U:
        printk("MODE_ETH_10GBASE_R CONFIG \n");
        break;
    case 1U:
        printk("MODE_ETH_USXGMII_10G CONFIG \n");
        break;
    case 3U:
        printk("MODE_ETH_USXGMII_5G CONFIG \n");
        break;
    case 4U:
        printk("MODE_ETH_USXGMII_2P5G CONFIG \n");
        break;
    case 5U:
        printk("MODE_ETH_HSGMII CONFIG \n");
        break;
    case 6U:
        printk("MODE_ETH_2P5BASE_X CONFIG \n");
        break;
    case 7U:
        printk("MODE_ETH_SGMII CONFIG \n");
        break;
    case 8U:
        printk("MODE_ETH_1GBASE_X CONFIG \n");
        break;
    default:
        printk("ERROR MODE CONFIG\n");
        break;
    }

    registers = (volatile uint32_t *)uni_serdes_base;
    for (offset = 0U; offset != 0x128U; offset += 4U)
        printk("pon serdes  addr:0x%x,value:0x%x\n",
               0x16100000U + offset, registers[offset / 4U]);

    return printk("#########################################################\n");
}

int uni_serdes_set_loopback_mode(uint32_t loopback_mode, int enable_aux_flags)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)uni_serdes_base;
    if (uni_loopback_count == 0U) {
        uni_loopback_default_1c = registers[0x1cU / 4U];
        uni_loopback_default_24 = registers[0x24U / 4U];
        uni_loopback_default_40 = registers[0x40U / 4U];
        uni_loopback_default_48 = registers[0x48U / 4U];
        uni_loopback_default_60 = registers[0x60U / 4U];
        uni_loopback_default_90 = registers[0x90U / 4U];
        uni_loopback_default_94 = registers[0x94U / 4U];
        printk("pon first time remmber default data\n");
    } else {
        registers[0x1cU / 4U] = uni_loopback_default_1c;
        registers[0x24U / 4U] = uni_loopback_default_24;
        registers[0x40U / 4U] = uni_loopback_default_40;
        registers[0x48U / 4U] = uni_loopback_default_48;
        registers[0x60U / 4U] = uni_loopback_default_60;
        registers[0x90U / 4U] = uni_loopback_default_90;
        registers[0x94U / 4U] = uni_loopback_default_94;
        printk("pon from the second time , should recovery the default data\n");
    }

    if (loopback_mode > 10U)
        return printk("the loop mode is big than PATH_MODE\n");
    if (isCpuType_133() != 1U)
        return (int)++uni_loopback_count;

    switch (loopback_mode) {
    case 0U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x00020000U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] =
            (registers[0x40U / 4U] & 0xfffe7fffU) | 0x00008000U;
        registers[0x48U / 4U] &= 0xffff8fffU;
        registers[0x48U / 4U] &= ~0x00020000U;
        registers[0x48U / 4U] |= 0x00040000U;
        registers[0x48U / 4U] &= ~0x00200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x00004000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 0x00000001U;
        if (enable_aux_flags == 1) {
            registers[0x94U / 4U] |= 0x0000e000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        printk("enter loop back PATH1_TX2RX_PCS_LOOP0 \n");
        break;
    case 1U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x00020000U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] =
            (registers[0x40U / 4U] & 0xfffe7fffU) | 0x00008000U;
        registers[0x48U / 4U] &= 0xffff8fffU;
        registers[0x48U / 4U] &= ~0x00020000U;
        registers[0x48U / 4U] |= 0x00040000U;
        registers[0x48U / 4U] &= ~0x00200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x00004000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 0x00000002U;
        if (enable_aux_flags == 1) {
            registers[0x94U / 4U] |= 0x0000e000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        printk("enter loop back PATH1_TX2RX_PCS_LOOP1 \n");
        break;
    case 2U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x00020000U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] =
            (registers[0x40U / 4U] & 0xfffe7fffU) | 0x00008000U;
        registers[0x48U / 4U] &= 0xffff8fffU;
        registers[0x48U / 4U] &= ~0x00020000U;
        registers[0x48U / 4U] |= 0x00040000U;
        registers[0x48U / 4U] &= ~0x00200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x00004000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 0x00000003U;
        if (enable_aux_flags == 1) {
            registers[0x94U / 4U] |= 0x0000e000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        printk("enter loop back PATH1_TX2RX_PCS_LOOP2 \n");
        break;
    case 3U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x00020000U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] &= 0xfffe7fffU;
        registers[0x48U / 4U] &= 0xffff8fffU;
        registers[0x48U / 4U] &= ~0x00020000U;
        registers[0x48U / 4U] |= 0x00040000U;
        registers[0x48U / 4U] &= ~0x00200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x00004000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 0x00000004U;
        if (enable_aux_flags == 1) {
            registers[0x94U / 4U] |= 0x0000e000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        printk("enter loop back PATH2_TX2RX_CABLE_LOOP \n");
        break;
    case 4U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x00020000U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xffe7ffffU) | 0x00080000U;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] =
            (registers[0x40U / 4U] & 0xfffe7fffU) | 0x00008000U;
        registers[0x48U / 4U] &= 0xffff8fffU;
        registers[0x48U / 4U] |= 0x00020000U;
        registers[0x48U / 4U] |= 0x00040000U;
        registers[0x48U / 4U] &= ~0x00200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x00004000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 0x00000005U;
        if (enable_aux_flags == 1) {
            registers[0x94U / 4U] |= 0x0000e000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        printk("enter loop back PATH3_TX2RX_PMA_LOOP \n");
        break;
    case 5U:
        registers[0x1cU / 4U] |= 0x100U;
        registers[0x1cU / 4U] &= 0xfffff9ffU;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] &= 0xfffe7fffU;
        registers[0x48U / 4U] &= ~0x00020000U;
        registers[0x48U / 4U] &= ~0x00040000U;
        registers[0x48U / 4U] &= ~0x00200000U;
        registers[0x60U / 4U] |= 0x00002000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x00004000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 0x00000006U;
        if (enable_aux_flags == 1) {
            registers[0x94U / 4U] |= 0x0000e000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        printk("enter loop back PATH4_RX2TX_PCS_LOOP \n");
        break;
    case 6U:
        registers[0x1cU / 4U] |= 0x100U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xff9fffffU) | 0x00200000U;
        registers[0x40U / 4U] &= 0xfffe7fffU;
        registers[0x48U / 4U] &= ~0x00020000U;
        registers[0x48U / 4U] &= ~0x00040000U;
        registers[0x48U / 4U] |= 0x00200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x00004000U;
        registers[0x94U / 4U] &= 0xfffffff8U;
        if (enable_aux_flags == 1) {
            registers[0x94U / 4U] |= 0x0000e000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        printk("enter loop back PATH5_RX2TX_PMA_LOOP \n");
        break;
    case 7U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x00020000U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] &= 0xfffe7fffU;
        registers[0x48U / 4U] =
            (registers[0x48U / 4U] & 0xffff8fffU) | 0x00002000U;
        registers[0x48U / 4U] &= ~0x00020000U;
        registers[0x48U / 4U] &= ~0x00040000U;
        registers[0x48U / 4U] &= ~0x00200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x00004000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 0x00000001U;
        if (enable_aux_flags == 1) {
            registers[0x94U / 4U] |= 0x0000e000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        printk("enter loop back PATH6_RX_RECEIVE \n");
        break;
    case 8U:
        registers[0x1cU / 4U] &= ~0x100U;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x00020000U;
        registers[0x24U / 4U] &= 0xffe7ffffU;
        registers[0x24U / 4U] &= 0xff9fffffU;
        registers[0x40U / 4U] &= 0xfffe7fffU;
        registers[0x48U / 4U] =
            (registers[0x48U / 4U] & 0xffff8fffU) | 0x00002000U;
        registers[0x48U / 4U] &= ~0x00020000U;
        registers[0x48U / 4U] &= ~0x00040000U;
        registers[0x48U / 4U] &= ~0x00200000U;
        registers[0x90U / 4U] =
            (registers[0x90U / 4U] & 0xffff9fffU) | 0x00004000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 0x00000001U;
        if (enable_aux_flags == 1) {
            registers[0x94U / 4U] |= 0x0000e000U;
            registers[0x94U / 4U] |= 0x80000000U;
        }
        printk("enter loop back PATH6_TRANSMIT \n");
        break;
    case 9U:
        printk("recovery the default data\n");
        break;
    default:
        printk("the path mode is error\n");
        break;
    }

    return (int)++uni_loopback_count;
}

int uni_serdes_set_tx_prbs_mode(int prbs_mode)
{
    volatile uint32_t *registers;

    uni_serdes_set_gen_en(1U);
    if (isCpuType_133() == 1U) {
        registers = (volatile uint32_t *)uni_serdes_base;
        registers[0x24U / 4U] =
            (registers[0x24U / 4U] & 0xfff8ffffU) | 0x00020000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 0x00000004U;
        registers[0x94U / 4U] |= 0x80000000U;
    } else if (isCpuType_129() == 1U) {
        registers = (volatile uint32_t *)uni_serdes_base;
        registers[0x24U / 4U] &= 0xfff8ffffU;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffcffU) | 0x00000200U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffff3ffU) | 0x00000800U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 0x00000004U;
        registers[0x94U / 4U] &= 0x7fffffffU;
    }

    registers = (volatile uint32_t *)uni_serdes_base;
    if (prbs_mode == 1) {
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfff8ffffU) | 0x00040000U;
        printk("\nserdes_set_tx_prbs_prbs_mode to 23\n");
    } else if (prbs_mode == 0) {
        registers[0x94U / 4U] &= 0xfff8ffffU;
        printk("\nserdes_set_tx_prbs_prbs_mode to 7\n");
    } else if (prbs_mode == 2) {
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfff8ffffU) | 0x00050000U;
        printk("\nserdes_set_tx_prbs_prbs_mode to 31\n");
    }

    return 0;
}

int uni_serdes_set_rx_prbs_mode(int prbs_mode)
{
    volatile uint32_t *registers;

    if (isCpuType_133() == 1U) {
        registers = (volatile uint32_t *)uni_serdes_base;
        registers[0x48U / 4U] =
            (registers[0x48U / 4U] & 0xffff8fffU) | 0x00002000U;
        registers[0x48U / 4U] &= ~0x00010000U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 0x00000004U;
        registers[0x94U / 4U] |= 0x80000000U;
    } else if (isCpuType_129() == 1U) {
        registers = (volatile uint32_t *)uni_serdes_base;
        registers[0x48U / 4U] &= 0xffff8fffU;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffcffU) | 0x00000200U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffff3ffU) | 0x00000800U;
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xfffffff8U) | 0x00000004U;
        registers[0x94U / 4U] &= 0x7fffffffU;
    }

    registers = (volatile uint32_t *)uni_serdes_base;
    if (prbs_mode == 1) {
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xffc7ffffU) | 0x00200000U;
        printk("\nserdes_set_tx_prbs_prbs_mode to 23\n");
    } else if (prbs_mode == 0) {
        registers[0x94U / 4U] &= 0xffc7ffffU;
        printk("\nserdes_set_tx_prbs_prbs_mode to 7\n");
    } else if (prbs_mode == 2) {
        registers[0x94U / 4U] =
            (registers[0x94U / 4U] & 0xffc7ffffU) | 0x00280000U;
        printk("\nserdes_set_tx_prbs_prbs_mode to 31\n");
    }

    return 0;
}

int uni_serdes_set_sprbsrxbist(int prbs_mode, int rx_bist_enable)
{
    volatile uint32_t *registers;

    registers = (volatile uint32_t *)uni_serdes_base;
    if (uni_rx_bist_count == 0U) {
        uni_rx_bist_default_from_24 = registers[0x24U / 4U];
        uni_rx_bist_default_48 = registers[0x48U / 4U];
        uni_rx_bist_default_94 = registers[0x94U / 4U];
        printk("first time remmber default data\n");
    }

    if (rx_bist_enable == 1) {
        uni_serdes_set_rx_prbs_mode(prbs_mode - 1);
        uni_serdes_set_check_en((uint32_t)rx_bist_enable);
        uni_serdes_set_err_cnt_en((uint32_t)rx_bist_enable);
        printk("%s PrbsMode=%d\n, RxBistEnable=%d\n",
               "uni_serdes_set_sprbsrxbist", prbs_mode, rx_bist_enable);
    } else {
        registers[0x14U / 4U] = uni_rx_bist_default_from_24;
        registers[0x48U / 4U] = uni_rx_bist_default_48;
        registers[0x94U / 4U] = uni_rx_bist_default_94;
        printk("recovery the default data\n");
    }

    return (int)++uni_rx_bist_count;
}

int uni_check_serdes_lock(void)
{
    volatile uint32_t *registers;
    uint32_t pll_status;
    uint32_t cdr_status;
    uint32_t alos_data;

    registers = (volatile uint32_t *)uni_serdes_base;
    if (isCpuType_129() == 1U)
        pll_status = (registers[0xccU / 4U] >> 1) & 1U;
    else
        pll_status = registers[0xd0U / 4U] & 1U;

    cdr_status = (registers[0xe4U / 4U] >> 1) & 1U;
    alos_data = registers[0xe4U / 4U] & 1U;
    return printk(" pll_sta=0x%x cdr_sta=0x%x alos_data=0x%x\n",
                  pll_status, cdr_status, alos_data);
}

int uni_serdes_get_hard_prbs_cnt(uint32_t time_units, int prbs_mode)
{
    uint32_t delay_count;
    uint64_t iteration;

    uni_serdes_err_cnt_reset();
    uni_serdes_set_error_time(time_units);
    uni_serdes_set_rx_prbs_mode(prbs_mode);
    uni_serdes_set_check_en(1U);
    uni_serdes_set_err_cnt_en(1U);
    uni_serdes_set_error_time_en(1U);

    delay_count = time_units * 1000U;
    for (iteration = 0U; iteration != delay_count; ++iteration)
        __const_udelay(0x418958UL);

    uni_iPrbsCounter += uni_serdes_get_err_cnt();
    return printk("uni_serdes_get_hard_prbs_cnt counter: %ld.\n", uni_iPrbsCounter);
}

int uni_serdes_get_prbs_counters(int time_units)
{
    volatile uint32_t *registers;
    uint32_t delay_ticks;

    printk("%s time = %d\n", "uni_serdes_get_prbs_counters", time_units);
    uni_serdes_set_error_time_en(0U);

    /* The result is unused, but the binary preserves this hardware read. */
    registers = (volatile uint32_t *)uni_serdes_base;
    (void)registers[0xe4U / 4U];

    del_timer(&uni_serdes_prbs_counter_timer);
    init_timer_key(&uni_serdes_prbs_counter_timer,
                   (void (*)(void))uni_serdesPrbsCounterGetHandler,
                   0U, 0U, 0U);
    delay_ticks = (uint32_t)time_units * 100U;
    uni_serdes_prbs_counter_timer.expires = jiffies + delay_ticks;
    add_timer(&uni_serdes_prbs_counter_timer);
    uni_serdes_err_cnt_reset();
    __const_udelay(0x418958UL);
    uni_serdesPrbsCounter = uni_serdes_get_err_cnt();
    return 0;
}

void uni_serdes_reset(uint32_t uni, uint32_t enable)
{
    if (uni > 1U || enable > 1U)
        printk("uni %d or en %d is error \n", uni, enable);

    if (enable == 0U) {
        if (uni == 0U) {
            apb_bit_write((volatile uint32_t *)uni_serdes_base, 0U, 1U, 12U);
            apb_bit_write((volatile uint32_t *)(uni_serdes_base + 0x400U),
                          0U, 1U, 8U);
            apb_bit_write((volatile uint32_t *)(uni_serdes_base + 0x400U),
                          0U, 1U, 9U);
        } else if (uni == 1U) {
            apb_bit_write((volatile uint32_t *)(uni_serdes_base + 0x200U),
                          0U, 1U, 12U);
            apb_bit_write((volatile uint32_t *)(uni_serdes_base + 0x400U),
                          0U, 1U, 6U);
            apb_bit_write((volatile uint32_t *)(uni_serdes_base + 0x400U),
                          0U, 1U, 7U);
        }
    } else if (enable == 1U) {
        if (uni == 0U) {
            apb_bit_write((volatile uint32_t *)(uni_serdes_base + 0x400U),
                          1U, 1U, 9U);
            __const_udelay(0x8312b0UL);
            apb_bit_write((volatile uint32_t *)uni_serdes_base, 1U, 1U, 12U);
            __const_udelay(0x418958UL);
            apb_bit_write((volatile uint32_t *)(uni_serdes_base + 0x400U),
                          1U, 1U, 8U);
        } else if (uni == 1U) {
            apb_bit_write((volatile uint32_t *)(uni_serdes_base + 0x400U),
                          1U, 1U, 7U);
            __const_udelay(0x8312b0UL);
            apb_bit_write((volatile uint32_t *)(uni_serdes_base + 0x200U),
                          1U, 1U, 12U);
            __const_udelay(0x418958UL);
            apb_bit_write((volatile uint32_t *)(uni_serdes_base + 0x400U),
                          1U, 1U, 6U);
        }
    }
}

int uni_serdes_set_tx_eq(uint32_t tx_eq)
{
    volatile uint32_t *tx_eq_register;

    if (tx_eq == 0U) {
        tx_eq_register = (volatile uint32_t *)(uni_serdes_base + 0x20U);
        *tx_eq_register = (*tx_eq_register & 0xffff00ffU) | 0x0d00U;
        printk("\nset tx 3db pre and post success \n");
    } else if (tx_eq == 1U) {
        tx_eq_register = (volatile uint32_t *)(uni_serdes_base + 0x20U);
        *tx_eq_register = (*tx_eq_register & 0xffff00ffU) | 0x1d00U;
        printk("\nset tx 6db pre and post success \n");
    }

    return 0;
}

int uni_serdes_set_pll_open_loop(uint32_t enable)
{
    volatile uint32_t *pll_control_register;
    volatile uint32_t *open_loop_register;
    uint32_t pll_control;

    pll_control_register = (volatile uint32_t *)(uni_serdes_base + 0x68U);
    open_loop_register = (volatile uint32_t *)(uni_serdes_base + 0x74U);
    pll_control = *pll_control_register;

    if (enable == 1U) {
        *pll_control_register = pll_control | 0x600U;
        *pll_control_register |= 0x400000U;
        *open_loop_register = (*open_loop_register & 0xffff9fffU) | 0x2000U;
        return printk("open pll open loop en =0x%x\n", enable);
    }

    *pll_control_register = pll_control & 0xfffff9ffU;
    *pll_control_register &= 0xffbfffffU;
    *open_loop_register &= 0xffff9fffU;
    return printk("close pll open loop en =0x%x\n", enable);
}

int uni_serdes_set_clk_change(uint32_t use_local_clock)
{
    volatile uint32_t *clock_select_register;

    clock_select_register = (volatile uint32_t *)(uni_serdes_base + 0x48U);
    if (use_local_clock == 0U) {
        *clock_select_register &= 0xfffbffffU;
        return printk("set rx to tx  clk looptiming clk\n");
    }

    *clock_select_register |= 0x40000U;
    return printk("set rx to tx  clk local clk\n");
}

void uni_serdes_set_rx_eq1(uint32_t enable, uint32_t equalizer_value)
{
    volatile uint32_t *equalizer_register;

    if (enable > 1U) {
        printk("serdes rx eq1 en is too big\n");
        return;
    }

    equalizer_register = (volatile uint32_t *)(uni_serdes_base + 0x2cU);
    if (enable == 0U) {
        *equalizer_register |= 1U;
        return;
    }

    *equalizer_register &= 0xfffffffeU;
    *equalizer_register =
        (*equalizer_register & 0xffffff07U) | (equalizer_value << 3);
}

void uni_serdes_set_rx_eq2(uint32_t enable, uint32_t equalizer_value)
{
    volatile uint32_t *equalizer_register;

    if (enable > 1U) {
        printk("serdes rx eq2 en is too big\n");
        return;
    }

    equalizer_register = (volatile uint32_t *)(uni_serdes_base + 0x2cU);
    if (enable == 0U) {
        *equalizer_register |= 2U;
        return;
    }

    *equalizer_register &= 0xfffffffdU;
    *equalizer_register =
        (*equalizer_register & 0xffffe0ffU) | (equalizer_value << 8);
}

void uni_serdes_set_rx_eq3(uint32_t enable, uint32_t equalizer_value)
{
    volatile uint32_t *equalizer_register;

    if (enable > 1U) {
        printk("serdes rx eq3 en is too big\n");
        return;
    }

    equalizer_register = (volatile uint32_t *)(uni_serdes_base + 0x2cU);
    if (enable == 0U) {
        *equalizer_register |= 4U;
        return;
    }

    *equalizer_register &= 0xfffffffbU;
    *equalizer_register =
        (*equalizer_register & 0xfffc1fffU) | (equalizer_value << 13);
}

void uni_mode_eth_10gbase_r_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_eth_10gbase_r_cfg\n");
    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x00U / 4U] = 0xe0000004U;
    registers[0x04U / 4U] = 0x50a840a3U;
    registers[0x08U / 4U] = 0x013e8687U;
    registers[0x0cU / 4U] = 0x0210c073U;
    registers[0x10U / 4U] = 0x00000048U;
    registers[0x14U / 4U] = 0x00000000U;
    registers[0x18U / 4U] = 0x00ff8000U;
    registers[0x1cU / 4U] = 0x00238020U;
    registers[0x20U / 4U] = 0x80000400U;
    registers[0x24U / 4U] = 0x00020000U;
    registers[0x28U / 4U] = 0x0c633830U;
    registers[0x2cU / 4U] = 0x00000e46U;
    registers[0x30U / 4U] = 0x00000020U;
    registers[0x34U / 4U] = 0x00002000U;
    registers[0x38U / 4U] = 0x00000000U;
    registers[0x3cU / 4U] = 0x00ff0000U;
    registers[0x40U / 4U] = 0x05a85100U;
    registers[0x44U / 4U] = 0x00000000U;
    registers[0x48U / 4U] = 0x4f042b2aU;
    registers[0x4cU / 4U] = 0x60002200U;
    registers[0x50U / 4U] = 0x34000003U;
    registers[0x54U / 4U] = 0x00000100U;
    registers[0x58U / 4U] = 0x00000080U;
    registers[0x5cU / 4U] = 0x00000000U;
    registers[0x60U / 4U] = 0x200554a8U;
    registers[0x64U / 4U] = 0x67748091U;
    registers[0x68U / 4U] = 0x01000000U;
    registers[0x6cU / 4U] = 0x40003b00U;
    registers[0x70U / 4U] = 0xa9004000U;
    registers[0x74U / 4U] = 0x10670002U;
    registers[0x78U / 4U] = 0x000000daU;
    registers[0x7cU / 4U] = 0xf20000c8U;
    registers[0x80U / 4U] = 0x10371037U;
    registers[0x84U / 4U] = 0x01000200U;
    registers[0x88U / 4U] = 0x00020001U;
    registers[0x8cU / 4U] = 0x10000000U;
    registers[0x90U / 4U] = 0x00084008U;
    registers[0x94U / 4U] = 0x802dc000U;
    registers[0x98U / 4U] = 0x000000ffU;
    registers[0x9cU / 4U] = 0x55555500U;
    registers[0xa0U / 4U] = 0x55555555U;
    registers[0xa4U / 4U] = 0x00555555U;
    registers[0xa8U / 4U] = 0x30000818U;
    registers[0xacU / 4U] = 0x40002000U;
    registers[0xb0U / 4U] = 0x0000000cU;
    registers[0xb4U / 4U] = 0x01000000U;
    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void uni_mode_eth_5gbase_r_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_eth_5gbase_r_cfg\n");
    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x00U / 4U] = 0xe0000004U;
    registers[0x04U / 4U] = 0x50a840a3U;
    registers[0x08U / 4U] = 0x013e864fU;
    registers[0x0cU / 4U] = 0x0210c073U;
    registers[0x10U / 4U] = 0x00000048U;
    registers[0x14U / 4U] = 0x00000000U;
    registers[0x18U / 4U] = 0x00ff8000U;
    registers[0x1cU / 4U] = 0x00238220U;
    registers[0x20U / 4U] = 0x80000200U;
    registers[0x24U / 4U] = 0x00020000U;
    registers[0x28U / 4U] = 0x0c633830U;
    registers[0x2cU / 4U] = 0x00000007U;
    registers[0x30U / 4U] = 0x00000020U;
    registers[0x34U / 4U] = 0x00002000U;
    registers[0x38U / 4U] = 0x00000000U;
    registers[0x3cU / 4U] = 0x00ff0000U;
    registers[0x40U / 4U] = 0x45a85100U;
    registers[0x44U / 4U] = 0x00000000U;
    registers[0x48U / 4U] = 0x4f002b2aU;
    registers[0x4cU / 4U] = 0x62002220U;
    registers[0x50U / 4U] = 0x34000003U;
    registers[0x54U / 4U] = 0x00000100U;
    registers[0x58U / 4U] = 0x00000080U;
    registers[0x5cU / 4U] = 0x00000000U;
    registers[0x60U / 4U] = 0x200574a8U;
    registers[0x64U / 4U] = 0x67748091U;
    registers[0x68U / 4U] = 0x01000000U;
    registers[0x6cU / 4U] = 0x40003b00U;
    registers[0x70U / 4U] = 0xa9004000U;
    registers[0x74U / 4U] = 0x10670002U;
    registers[0x78U / 4U] = 0x000000daU;
    registers[0x7cU / 4U] = 0xf20000c8U;
    registers[0x80U / 4U] = 0x10371037U;
    registers[0x84U / 4U] = 0x01000200U;
    registers[0x88U / 4U] = 0x00020001U;
    registers[0x8cU / 4U] = 0x10000000U;
    registers[0x90U / 4U] = 0x00084008U;
    registers[0x94U / 4U] = 0x802dc000U;
    registers[0x98U / 4U] = 0x000000ffU;
    registers[0x9cU / 4U] = 0x55555500U;
    registers[0xa0U / 4U] = 0x55555555U;
    registers[0xa4U / 4U] = 0x00555555U;
    registers[0xa8U / 4U] = 0x30000818U;
    registers[0xacU / 4U] = 0x40002000U;
    registers[0xb0U / 4U] = 0x0000000cU;
    registers[0xb4U / 4U] = 0x01000000U;
    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void uni_mode_eth_2p5gbase_r_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_eth_2p5gbase_r_cfg\n");
    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x00U / 4U] = 0xe0000004U;
    registers[0x04U / 4U] = 0x50a840a3U;
    registers[0x08U / 4U] = 0x013e8620U;
    registers[0x0cU / 4U] = 0x0210c073U;
    registers[0x10U / 4U] = 0x00000048U;
    registers[0x14U / 4U] = 0x00000000U;
    registers[0x18U / 4U] = 0x00ff8000U;
    registers[0x1cU / 4U] = 0x00238420U;
    registers[0x20U / 4U] = 0x80000100U;
    registers[0x24U / 4U] = 0x00020000U;
    registers[0x28U / 4U] = 0x0c633830U;
    registers[0x2cU / 4U] = 0x00000007U;
    registers[0x30U / 4U] = 0x00000020U;
    registers[0x34U / 4U] = 0x00002000U;
    registers[0x38U / 4U] = 0x00000000U;
    registers[0x3cU / 4U] = 0x00ff0000U;
    registers[0x40U / 4U] = 0x85a85100U;
    registers[0x44U / 4U] = 0x00000000U;
    registers[0x48U / 4U] = 0x4f002b2aU;
    registers[0x4cU / 4U] = 0x64002220U;
    registers[0x50U / 4U] = 0x34000003U;
    registers[0x54U / 4U] = 0x00000100U;
    registers[0x58U / 4U] = 0x00000080U;
    registers[0x5cU / 4U] = 0x00000000U;
    registers[0x60U / 4U] = 0x200574a8U;
    registers[0x64U / 4U] = 0x67748091U;
    registers[0x68U / 4U] = 0x01000000U;
    registers[0x6cU / 4U] = 0x40003b00U;
    registers[0x70U / 4U] = 0xa9004000U;
    registers[0x74U / 4U] = 0x10670002U;
    registers[0x78U / 4U] = 0x000000daU;
    registers[0x7cU / 4U] = 0xf20000c8U;
    registers[0x80U / 4U] = 0x10371037U;
    registers[0x84U / 4U] = 0x01000200U;
    registers[0x88U / 4U] = 0x00020001U;
    registers[0x8cU / 4U] = 0x10000000U;
    registers[0x90U / 4U] = 0x00084008U;
    registers[0x94U / 4U] = 0x802dc000U;
    registers[0x98U / 4U] = 0x000000ffU;
    registers[0x9cU / 4U] = 0x55555500U;
    registers[0xa0U / 4U] = 0x55555555U;
    registers[0xa4U / 4U] = 0x00555555U;
    registers[0xa8U / 4U] = 0x30000818U;
    registers[0xacU / 4U] = 0x40002000U;
    registers[0xb0U / 4U] = 0x0000000cU;
    registers[0xb4U / 4U] = 0x01000000U;
    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void uni_mode_eth_2p5gbase_x_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_eth_2p5gbase_x_cfg\n");
    if (isCpuType_133() == 1U) {
        registers = (volatile uint32_t *)uni_serdes_base;
        registers[0x00U / 4U] = 0xe0000004U;
        registers[0x04U / 4U] = 0x4fa8c0a2U;
        registers[0x08U / 4U] = 0x013e8604U;
        registers[0x0cU / 4U] = 0x1f51c8f3U;
        registers[0x10U / 4U] = 0x00000044U;
        registers[0x14U / 4U] = 0x00194000U;
        registers[0x18U / 4U] = 0x0b080000U;
        registers[0x1cU / 4U] = 0x00238220U;
        registers[0x20U / 4U] = 0x80000100U;
        registers[0x24U / 4U] = 0x00050000U;
        registers[0x28U / 4U] = 0x0c633830U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0x05a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x04005b6aU;
        registers[0x4cU / 4U] = 0x60002220U;
        registers[0x50U / 4U] = 0x34000003U;
        registers[0x54U / 4U] = 0x00000408U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200574a8U;
        registers[0x64U / 4U] = 0x00649052U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x40007700U;
        registers[0x70U / 4U] = 0xa9004000U;
        registers[0x74U / 4U] = 0x01108002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x10371037U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x802dc000U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x55555500U;
        registers[0xa0U / 4U] = 0x55555555U;
        registers[0xa4U / 4U] = 0x00555555U;
        registers[0xa8U / 4U] = 0x30000818U;
        registers[0xacU / 4U] = 0x40002000U;
    } else {
        if (isCpuType_129() != 1U)
            return;

        registers = (volatile uint32_t *)uni_serdes_base;
        registers[0x00U / 4U] = 0x00000004U;
        registers[0x04U / 4U] = 0x000040a0U;
        registers[0x08U / 4U] = 0x00008009U;
        registers[0x0cU / 4U] = 0x1f500000U;
        registers[0x10U / 4U] = 0x00000004U;
        registers[0x14U / 4U] = 0x00194000U;
        registers[0x18U / 4U] = 0x00000000U;
        registers[0x1cU / 4U] = 0x00000220U;
        registers[0x20U / 4U] = 0x80000200U;
        registers[0x24U / 4U] = 0x00000000U;
        registers[0x28U / 4U] = 0x00000010U;
        registers[0x2cU / 4U] = 0x00000421U;
        registers[0x30U / 4U] = 0x00000000U;
        registers[0x34U / 4U] = 0x00000000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00000000U;
        registers[0x40U / 4U] = 0x05a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x04040b6aU;
        registers[0x4cU / 4U] = 0x80002200U;
        registers[0x50U / 4U] = 0x00000604U;
        registers[0x54U / 4U] = 0x00000400U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x3de49092U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0xc0003700U;
        registers[0x70U / 4U] = 0xa9004000U;
        registers[0x74U / 4U] = 0x10108000U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x10371037U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x00004000U;
        registers[0x94U / 4U] = 0x00000000U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x33333300U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x00333333U;
        registers[0xa8U / 4U] = 0x20000818U;
        registers[0xacU / 4U] = 0x0000201cU;
    }

    registers[0xb0U / 4U] = 0x0000000cU;
    registers[0xb4U / 4U] = 0x01000000U;
    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void uni_mode_eth_1gbase_x_cfg(void)
{
    volatile uint32_t *registers;

    printk("mode_eth_1gbase_x_cfg\n");
    if (isCpuType_133() == 1U) {
        registers = (volatile uint32_t *)uni_serdes_base;
        registers[0x00U / 4U] = 0xe0000004U;
        registers[0x04U / 4U] = 0x4fa840a3U;
        registers[0x08U / 4U] = 0x013e860fU;
        registers[0x0cU / 4U] = 0x0210c073U;
        registers[0x10U / 4U] = 0x00000048U;
        registers[0x14U / 4U] = 0x00000000U;
        registers[0x18U / 4U] = 0x00ff8000U;
        registers[0x1cU / 4U] = 0x00238620U;
        registers[0x20U / 4U] = 0x80000000U;
        registers[0x24U / 4U] = 0x00050000U;
        registers[0x28U / 4U] = 0x0c633830U;
        registers[0x2cU / 4U] = 0x00000007U;
        registers[0x30U / 4U] = 0x00000020U;
        registers[0x34U / 4U] = 0x00002000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00ff0000U;
        registers[0x40U / 4U] = 0xc5a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x01045b2aU;
        registers[0x4cU / 4U] = 0x66002200U;
        registers[0x50U / 4U] = 0x34000003U;
        registers[0x54U / 4U] = 0x00000100U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x00648091U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0x00003700U;
        registers[0x70U / 4U] = 0xa9004000U;
        registers[0x74U / 4U] = 0x10640002U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x10371037U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x0000001cU;
        registers[0x94U / 4U] = 0x8000c000U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x55555500U;
        registers[0xa0U / 4U] = 0x55555555U;
        registers[0xa4U / 4U] = 0x00555555U;
        registers[0xa8U / 4U] = 0x30000818U;
        registers[0xacU / 4U] = 0x40002000U;
    } else {
        if (isCpuType_129() != 1U)
            return;

        registers = (volatile uint32_t *)uni_serdes_base;
        registers[0x00U / 4U] = 0x00000004U;
        registers[0x04U / 4U] = 0x000040a0U;
        registers[0x08U / 4U] = 0x00008003U;
        registers[0x0cU / 4U] = 0x0c900000U;
        registers[0x10U / 4U] = 0x00000004U;
        registers[0x14U / 4U] = 0x00150000U;
        registers[0x18U / 4U] = 0x00000000U;
        registers[0x1cU / 4U] = 0x00000220U;
        registers[0x20U / 4U] = 0x80000200U;
        registers[0x24U / 4U] = 0x00000000U;
        registers[0x28U / 4U] = 0x00000010U;
        registers[0x2cU / 4U] = 0x00000421U;
        registers[0x30U / 4U] = 0x00000000U;
        registers[0x34U / 4U] = 0x00000000U;
        registers[0x38U / 4U] = 0x00000000U;
        registers[0x3cU / 4U] = 0x00000000U;
        registers[0x40U / 4U] = 0x05a85100U;
        registers[0x44U / 4U] = 0x00000000U;
        registers[0x48U / 4U] = 0x01040b6aU;
        registers[0x4cU / 4U] = 0x60002200U;
        registers[0x50U / 4U] = 0x00000603U;
        registers[0x54U / 4U] = 0x00000400U;
        registers[0x58U / 4U] = 0x00000080U;
        registers[0x5cU / 4U] = 0x00000000U;
        registers[0x60U / 4U] = 0x200554a8U;
        registers[0x64U / 4U] = 0x3de49092U;
        registers[0x68U / 4U] = 0x01000000U;
        registers[0x6cU / 4U] = 0xc0003700U;
        registers[0x70U / 4U] = 0xa9004000U;
        registers[0x74U / 4U] = 0x10108000U;
        registers[0x78U / 4U] = 0x000000daU;
        registers[0x7cU / 4U] = 0xf20000c8U;
        registers[0x80U / 4U] = 0x10371037U;
        registers[0x84U / 4U] = 0x01000200U;
        registers[0x88U / 4U] = 0x00020001U;
        registers[0x8cU / 4U] = 0x10000000U;
        registers[0x90U / 4U] = 0x00004000U;
        registers[0x94U / 4U] = 0x00000000U;
        registers[0x98U / 4U] = 0x000000ffU;
        registers[0x9cU / 4U] = 0x33333300U;
        registers[0xa0U / 4U] = 0x33333333U;
        registers[0xa4U / 4U] = 0x00333333U;
        registers[0xa8U / 4U] = 0x20000818U;
        registers[0xacU / 4U] = 0x0000201cU;
    }

    registers[0xb0U / 4U] = 0x0000000cU;
    registers[0xb4U / 4U] = 0x01000000U;
    registers[0xb8U / 4U] = 0x00000080U;
    registers[0xbcU / 4U] = 0x00010000U;
    registers[0xc0U / 4U] = 0x00000000U;
}

void uni_serdes_mode_set(uint32_t mode)
{
    switch (mode) {
    case 0U:
    case 1U:
        uni_mode_eth_10gbase_r_cfg();
        break;
    case 2U:
    case 3U:
        uni_mode_eth_5gbase_r_cfg();
        break;
    case 4U:
        uni_mode_eth_2p5gbase_r_cfg();
        break;
    case 5U:
    case 6U:
        uni_mode_eth_2p5gbase_x_cfg();
        break;
    case 7U:
    case 8U:
        uni_mode_eth_1gbase_x_cfg();
        break;
    default:
        return;
    }
}

int uni_zx_serdes_init(uint32_t mode)
{
    volatile uint32_t *registers;
    unsigned int retries;

    uni_serdes_mode_set(mode);
    retries = 1001U;
    printk("exit uni_serdes_mode_set \n");

    registers = (volatile uint32_t *)uni_serdes_base;
    registers[0x54U / 4U] |= 1U;

    if (isCpuType_133() == 1U) {
        if (mode - 5U <= 1U) {
            while ((registers[0xccU / 4U] & 7U) != 7U) {
                __const_udelay(0x8312b0UL);
                if (--retries == 0U) {
                    printk("sc pll lock failed!\n");
                    return -1;
                }
            }
            printk("sc_pll_lock_ready\n");
        } else {
            while ((registers[0xd0U / 4U] & 1U) == 0U) {
                __const_udelay(0x8312b0UL);
                if (--retries == 0U) {
                    printk("com pll lock failed!\n");
                    return -1;
                }
            }
            printk("com_pll_lock_ready\n");
        }
    } else {
        while ((registers[0xccU / 4U] & 2U) == 0U) {
            __const_udelay(0x8312b0UL);
            if (--retries == 0U) {
                printk("com pll lock failed!\n");
                return -1;
            }
        }
        printk("com_pll_lock_ready\n");
    }

    if ((registers[0xe4U / 4U] & 1U) != 0U)
        printk("rx los =1 no  rx data in\n");
    else
        printk("rx los =0 rx data in\n");

    retries = 1001U;
    while ((registers[0xe4U / 4U] & 2U) == 0U) {
        __const_udelay(0x8312b0UL);
        if (--retries == 0U) {
            printk("cdr lock failed!\n");
            return -1;
        }
    }
    printk("cdr_lock_ready\n");

    if (isCpuType_129() == 1U) {
        retries = 1001U;
        while ((registers[0xe4U / 4U] & 0x200U) == 0U &&
               (registers[0xe4U / 4U] & 0x400U) == 0U) {
            __const_udelay(0x8312b0UL);
            if (--retries == 0U) {
                printk("mux_txpcs_rst_n or mux_rxpcs_rst_n failed!\n");
                return -1;
            }
        }
        printk("serdes clock send out\n");
    }

    return 0;
}

int uni_pll_cfg(uint32_t mode)
{
    volatile uint32_t *registers;

    if (mode <= 4U) {
        printk("enter epon pon pll cfg \n");
        registers = (volatile uint32_t *)top_crm_base;
        registers[0x04U / 4U] = (registers[0x04U / 4U] & 0xffffffcfU) | 0x20U;
        registers[0xc0U / 4U] = 0x00202855U;
        registers[0xc4U / 4U] = 0x0a000000U;
        registers[0xc4U / 4U] |= 0x10000000U;
        registers[0x0cU / 4U] |= 0x200U;
        registers[0x0cU / 4U] &= 0xfffffeffU;
    }

    if (mode - 5U <= 2U) {
        printk("enter gpon pon pll cfg \n");
        registers = (volatile uint32_t *)top_crm_base;
        registers[0x04U / 4U] = (registers[0x04U / 4U] & 0xffffffcfU) | 0x20U;
        registers[0xc0U / 4U] = 0x20202054U;
        registers[0xc4U / 4U] = 0x0a2673e3U;
        registers[0xc4U / 4U] |= 0x10000000U;
        registers[0x0cU / 4U] |= 0x200U;
        registers[0x0cU / 4U] &= 0xfffffeffU;
    }

    return 0;
}

uint32_t uni_eth_mode_change(uint32_t mode)
{
    switch (mode) {
    case 0U:
        return 13U;
    case 1U:
        return 14U;
    case 2U:
        return 11U;
    case 3U:
        return 12U;
    case 4U:
        return 10U;
    case 5U:
        return 9U;
    case 6U:
        return 15U;
    case 7U:
        return 8U;
    case 8U:
        return 16U;
    default:
        printk("uni_eth_mode_change error ! Can't find this mode[%#x]!\n", mode);
        return 0U;
    }
}

int uni_serdes_init(uint8_t xmac, uint32_t mode)
{
    volatile uint32_t *registers;
    unsigned int delay_count;
    uint32_t reset_mode;

    if (xmac == 0U) {
        uni_serdes_mode = mode;
        registers = (volatile uint32_t *)top_crm_base;
        registers[0x70U / 4U] &= 0xffffffefU;
        registers[0x70U / 4U] &= 0xffffffdfU;
        delay_count = 11U;
        while (--delay_count != 0U)
            __const_udelay(0x418958UL);

        registers[0x70U / 4U] |= 0x10U;
        registers[0x70U / 4U] |= 0x20U;
        delay_count = 11U;
        while (--delay_count != 0U)
            __const_udelay(0x418958UL);

        if (uni_zx_serdes_init(mode) != 0)
            printk("serdes init failed because the pll or cdr is not lock\n");
        else
            printk("uni serdes init succeed\n");
    } else if (xmac == 1U) {
        if (g_ponserdes_to_xmac1 == 1U) {
            reset_mode = uni_eth_mode_change(mode);
            if (reset_mode != 0U)
                zx_pon_clk_reset_init(reset_mode);
        } else {
            printk("pon serdes is used for pon mac\n");
        }
    }

    return 0;
}

uint32_t uni_serdes_on_133(void)
{
    volatile uint32_t *register_word;
    uint32_t value;

    register_word = (volatile uint32_t *)(uni_serdes_base + 0x54U);
    value = *register_word | 1U;
    *register_word = value;
    return value;
}

uint64_t __fswab64(uint64_t value)
{
    return ((value & UINT64_C(0x00000000000000ff)) << 56) |
           ((value & UINT64_C(0x000000000000ff00)) << 40) |
           ((value & UINT64_C(0x0000000000ff0000)) << 24) |
           ((value & UINT64_C(0x00000000ff000000)) << 8) |
           ((value & UINT64_C(0x000000ff00000000)) >> 8) |
           ((value & UINT64_C(0x0000ff0000000000)) >> 24) |
           ((value & UINT64_C(0x00ff000000000000)) >> 40) |
           ((value & UINT64_C(0xff00000000000000)) >> 56);
}

int _idm_rx_refill(unsigned int *descriptor_address, unsigned int size)
{
    uint8_t *buffer;
    uintptr_t physical_address;

    buffer = idm_alloc_buf(size);
    if (buffer == 0)
        return -1;

    physical_address = virt_to_phys_0(buffer + uBP_BUFFER_OFFSET + 64U);
    *descriptor_address = __fswab32_1(physical_address);
    return 0;
}

unsigned int getEponDeactiveState(void)
{
    return g_epon_deactive;
}

unsigned int *setEponDeactiveState(int state)
{
    g_epon_deactive = state != 0;
    return &g_epon_deactive;
}

int pon_set_8k_out_en(uint8_t enable)
{
    uint32_t pin_mux_value;

    pin_mux_value = *pin_mux_base;
    if (enable == 1U) {
        *pin_mux_base = (pin_mux_value & 0xfffffff9U) | 2U;
    } else if (((pin_mux_value >> 1) & 3U) == 1U) {
        *pin_mux_base = pin_mux_value | 6U;
    }
    return 0;
}

int pon_set_1pps_out_en(uint8_t enable)
{
    uint32_t pin_mux_value;

    pin_mux_value = pin_mux_base[2];
    if (enable == 1U) {
        pin_mux_base[2] = (pin_mux_value & 0xff1fffffU) | 0x00400000U;
    } else if (((pin_mux_value >> 21) & 7U) == 2U) {
        pin_mux_base[2] = pin_mux_value | 0x00e00000U;
    }
    return 0;
}

int pon_set_uart1_txd_en(uint8_t enable)
{
    uint32_t pin_mux_value;

    pin_mux_value = *pin_mux_base;
    if (enable == 1U) {
        *pin_mux_base = pin_mux_value & 0xe7ffffffU;
    } else if ((pin_mux_value & 0x18000000U) == 0U) {
        *pin_mux_base = (pin_mux_value & 0xe7ffffffU) | 0x10000000U;
    }
    return 0;
}

int pon_set_1pps_tod_out_en(uint8_t enable)
{
    pon_set_1pps_out_en(enable);
    pon_set_uart1_txd_en(enable);
    return 0;
}

int pon_set_pin_mux_13(uint8_t value)
{
    uint32_t pin_mux_value;

    pin_mux_value = *pin_mux_base;
    *pin_mux_base = (pin_mux_value & 0xf9ffffffU) |
                    (((uint32_t)value & 3U) << 25);
    return 0;
}

int pon_set_pll_pon_ref_clock(uint8_t clock)
{
    uint32_t control_value;

    control_value = top_crm_base[4];
    top_crm_base[4] = (control_value & 0xffffffcfU) |
                      (((uint32_t)clock & 3U) << 4);
    return 0;
}

int pon_set_pll_pon_cfg_with_ref_clk_25M(void)
{
    top_crm_base[0xc4U / sizeof(*top_crm_base)] = 0x0a000000U;
    top_crm_base[0xc0U / sizeof(*top_crm_base)] = 0x00102371U;
    top_crm_base[0xc8U / sizeof(*top_crm_base)] = 0x20U;
    top_crm_base[0xccU / sizeof(*top_crm_base)] = 0U;
    top_crm_base[0xc4U / sizeof(*top_crm_base)] |= 0x10000000U;
    return 0;
}

int pon_set_pll_pon_en(uint8_t enable)
{
    uint32_t control_value;

    control_value = top_crm_base[0xc4U / sizeof(*top_crm_base)];
    top_crm_base[0xc4U / sizeof(*top_crm_base)] =
        (control_value & 0xefffffffU) | (((uint32_t)enable & 1U) << 28);
    return 0;
}

int pon_use_pll_pon_ref_from_ex_pll(void)
{
    pon_set_8k_out_en(1U);
    pon_set_pin_mux_13(1U);
    pon_set_pll_pon_ref_clock(1U);
    pon_set_pll_pon_cfg_with_ref_clk_25M();
    return 0;
}

unsigned int isCpuType_133(void)
{
    return g_pon_cputype == 2U;
}

unsigned int isCpuType_132(void)
{
    return g_pon_cputype == 1U;
}

unsigned int isCpuType_129(void)
{
    return g_pon_cputype == 4U;
}

unsigned int *ponserdes_to_xmac1_en_set(unsigned int enable)
{
    volatile uint32_t *nppt_control_register;
    unsigned int inverse_enable;

    if (enable <= 1U)
        (void)greg_sdet_share_clk_cfg(enable);

    inverse_enable = enable != 1U;
    *(volatile uint32_t *)(pon_base + 0x80U) = inverse_enable;
    nppt_control_register = (volatile uint32_t *)(nppt_base + 0x2438U);
    *nppt_control_register = (*nppt_control_register & 0xfffffffbu) |
                             (inverse_enable << 2);
    g_ponserdes_to_xmac1 = enable == 1U;
    return &g_epon_deactive;
}

int pon_sys_soft_reset(void)
{
    volatile uint32_t *reset_register;
    uint32_t reset_value;

    reset_register = (volatile uint32_t *)(nppt_base + 0x2c0004U);
    reset_value = *reset_register;
    printk("val =0x%x, reg = 0x%px\n", reset_value,
           (const void *)reset_register);
    reset_value &= 0x7fffffffU;
    *reset_register = reset_value;
    printk("reset val =0x%x\n", reset_value);
    __const_udelay(1718000UL);
    reset_value |= 0x80000000U;
    *reset_register = reset_value;
    printk("pon_sys_soft_reset restore val = 0x%x\n", reset_value);
    return 0;
}

unsigned int arm64_kernel_use_ng_mappings(void)
{
    if (arm64_const_caps_ready > 0)
        return cpu_hwcap_keys[23] > 0;
    return (unsigned int)((cpu_hwcaps >> 23) & 1U);
}

unsigned int pon_int_enable(unsigned int mask)
{
    volatile uint32_t *interrupt_mask_register;
    uint32_t interrupt_mask;

    interrupt_mask_register = (volatile uint32_t *)(pon_base + 0x44U);
    interrupt_mask = *interrupt_mask_register & ~mask;
    *interrupt_mask_register = interrupt_mask;
    return interrupt_mask;
}

unsigned int nppt_int_enable(unsigned int mask)
{
    volatile uint32_t *interrupt_mask_register;
    uint32_t interrupt_mask;

    interrupt_mask_register = (volatile uint32_t *)(nppt_base + 4U);
    interrupt_mask = *interrupt_mask_register & ~mask;
    *interrupt_mask_register = interrupt_mask;
    return interrupt_mask;
}

unsigned int register_gmac_int(pon_interrupt_callback_t callback,
                               uintptr_t context)
{
    gpon_isr = callback;
    pon_int_info = context;
    return pon_int_enable(1U);
}

unsigned int register_xgmac_int(pon_interrupt_callback_t callback,
                                uintptr_t context)
{
    xgpon_isr = callback;
    pon_int_info = context;
    return pon_int_enable(0x80U);
}

unsigned int register_emac_int(pon_interrupt_callback_t callback,
                               uintptr_t context)
{
    epon_isr = callback;
    pon_int_info = context;
    return pon_int_enable(0x100U);
}

unsigned int register_xeumac_int(pon_interrupt_callback_t callback,
                                 uintptr_t context)
{
    xeupon_isr = callback;
    pon_int_info = context;
    return pon_int_enable(0x200U);
}

unsigned int register_xedmac_int(pon_interrupt_callback_t callback,
                                 uintptr_t context)
{
    xedpon_isr = callback;
    pon_int_info = context;
    return pon_int_enable(0x400U);
}

unsigned int register_dg_int(uintptr_t handler, uintptr_t context)
{
    dg_isr = handler;
    pon_int_info = context;
    return pon_int_enable(0x20U);
}

unsigned int register_lp_int(pon_interrupt_callback_t callback,
                             uintptr_t context)
{
    lp_isr = callback;
    pon_int_info = context;
    return pon_int_enable(0x40U);
}

unsigned int register_low_power_int(pon_interrupt_callback_t callback,
                                    uintptr_t context)
{
    low_power_isr = callback;
    pon_int_info = context;
    return pon_int_enable(0x800U);
}

unsigned int register_ptp_int(pon_interrupt_callback_t callback,
                              uintptr_t context)
{
    ptp_isr = callback;
    pon_int_info = context;
    return nppt_int_enable(0x400U);
}

unsigned int register_ptp_stamp_int(pon_interrupt_callback_t callback,
                                    uintptr_t context)
{
    ptp_stamp_isr = callback;
    pon_int_info = context;
    return nppt_int_enable(0x200U);
}

unsigned int register_oam_int(pon_interrupt_callback_t callback)
{
    oam_isr = callback;
    return nppt_int_enable(0x100U);
}

int pon_soc_pon_core_clk_init(void)
{
    uint32_t control_value;

    if (isCpuType_133() != 1U && isCpuType_129() != 1U)
        return 0;

    control_value = top_crm_base[3];
    if (isCpuType_129() == 1U)
        top_crm_base[3] = (control_value & 0xffffffcfU) | 0x20U;
    else
        top_crm_base[3] = control_value | 0x70U;
    return 0;
}

int pon_soc_pon_cci_clk_init(void)
{
    top_crm_base[1] |= 0x30U;
    return 0;
}

int pon_soc_pon_woe0_clk_init(void)
{
    top_crm_base[3] |= 0x00700000U;
    return 0;
}

int pon_soc_pon_woe1_clk_init(void)
{
    top_crm_base[3] |= 0x07000000U;
    return 0;
}

int pon_soc_pon_tm_clk_init(void)
{
    top_crm_base[3] |= 3U;
    return 0;
}

int nppt_idm_cci_enable(void)
{
    *(volatile uint32_t *)(sys_ctrl_base + 0x78U) = 0x00200020U;
    *(volatile uint32_t *)(sys_ctrl_base + 0x7cU) = 0x00200020U;
    return printk("idm cci enable\n");
}

int pon_soc_pon_cci_aclk_init(void)
{
    top_crm_base[1] |= 0x70U;
    return 0;
}

int pon_soc_pon_tm_aclk_init(void)
{
    if (isCpuType_129() == 1U)
        top_crm_base[3] |= 3U;
    else
        top_crm_base[3] |= 7U;
    return 0;
}

int pon_soc_pon_nppt_clk_init(void)
{
    uint32_t mux_value;
    uint32_t enable_value;

    mux_value = top_crm_base[3];
    enable_value = top_crm_base[0x48U / sizeof(*top_crm_base)];
    if (isCpuType_129() == 1U)
        mux_value = (mux_value & 0xff9fffcfU) | 0x00400030U;
    else
        mux_value = (mux_value & 0xf8ffffffU) | 0x06000000U;
    enable_value |= 0x400U;
    top_crm_base[3] = mux_value;
    top_crm_base[0x48U / sizeof(*top_crm_base)] = enable_value;
    printk("CLK_MUX_3 is 0x%.8x,CLK_EN_6 is 0x%.8x.\n",
           top_crm_base[3], top_crm_base[0x48U / sizeof(*top_crm_base)]);
    return 0;
}

int pon_soc_pon_woe_clk_init(void)
{
    if (isCpuType_129() == 1U)
        top_crm_base[3] |= 0x000c0000U;
    else
        top_crm_base[3] |= 0x00700000U;
    return 0;
}

int pon_soc_pon_rgmii_clk_set(int enable)
{
    uint32_t control_value;

    control_value = top_crm_base[3] & 0xfffeffffU;
    if (enable == 0)
        control_value |= 0x00010000U;
    top_crm_base[3] = control_value;
    return 0;
}

int pps_reset(void)
{
    volatile uint32_t *reset_register;
    uint32_t reset_value;

    reset_register = (volatile uint32_t *)(pps_base + 0xcU);
    reset_value = *reset_register;
    printk("val =0x%x, reg = 0x%px\n", reset_value,
           (const void *)reset_register);
    reset_value &= 0xfff85400U;
    *reset_register = reset_value;
    printk("reset val =0x%x\n", reset_value);
    reset_value |= 0x0007abffU;
    __const_udelay(1718000UL);
    *reset_register = reset_value;
    printk("restore val = 0x%x\n", reset_value);
    return 0;
}

int phy_zxic_051_phy_uni_check(uint8_t phy, unsigned int *phy_state,
                                uint8_t *phy_link, uint8_t *outer_speed,
                                uint8_t *duplex)
{
    uint8_t phy_id = phy_051_g_phy_id_check(phy);
    uint8_t port_index;
    int32_t mdio_slot;
    uint16_t status_register;
    uint16_t register_value;

    *phy_state = 0U;
    *phy_link = 0U;
    *outer_speed = 0U;
    *duplex = 0U;
    if (phy <= 3U) {
        if (__printk_ratelimit("phy_zxic_051_phy_uni_check") != 0)
            printk("<%s>(%d)port%d out of range err!\n",
                   "phy_zxic_051_phy_uni_check", 351, phy);
        return -1;
    }

    mdio_slot = (int32_t)phy - 4;
    port_index = (uint8_t)mdio_slot;
    if (nbaset_flag[port_index] == 1U)
        return 0;
    if (phy_id == 0xffU) {
        if (__printk_ratelimit("phy_zxic_051_phy_uni_check") != 0)
            printk("051 phyid get err!\n");
        return -1;
    }

    status_register = zx_mdio_read_ge_ext_by_port[mdio_slot](phy_id, 26U);
    if (status_register == 0xffffU)
        return -1;
    *phy_link = (uint8_t)((status_register >> 6) & 1U);

    if (*phy_link != 0U) {
        phy_zxic_051_uniserdes_mode_check(phy, status_register, outer_speed,
                                           duplex);
        high_ber_downspeed_check(phy, phy_id, *phy_link, *outer_speed);
        outerphy_link[port_index] = 1U;
        outerphy_link[port_index + 4U] = *outer_speed;
        outerphy_link[port_index + 8U] = *duplex;
        if (sg_linkdn_check_times_mac[port_index] == 0U) {
            phy_zxic_051_apb_write(phy, phy_id, 84U, 1025U);
            sg_linkdn_check_times_mac[port_index] = 2U;
        }

        if (*outer_speed == 3U) {
            if (sg_reg_val_8017_mac[port_index] == 1U) {
                if (outerphy_link[port_index + 16U] == 1U) {
                    sg_reg_val_8017_mac[port_index] = 0U;
                    outerphy_link[port_index + 16U] = 0U;
                    zx_mdio_write_extended[mdio_slot](phy_id, 1U,
                                                       0xffff801eU, 0U);
                    zx_mdio_write_extended[mdio_slot](phy_id, 31U,
                                                       0xffff80ccU, 70U);
                    printk("<port:%d> reg1.8017 write 0x0\n", phy);
                } else {
                    outerphy_link[port_index + 16U] = 1U;
                }
            }
        } else if (sg_reg_val_8017_mac[port_index] == 0U) {
            sg_reg_val_8017_mac[port_index] = 1U;
            zx_mdio_write_extended[mdio_slot](phy_id, 1U, 0xffff801eU, 180U);
            zx_mdio_write_extended[mdio_slot](phy_id, 31U, 0xffff80ccU, 127U);
            printk("<port:%d> reg1.8017 write 0x1\n", phy);
        }

        *phy_state = *outer_speed | ((*duplex & 1U) << 10);
        speed_hold_check(phy, phy_id, *phy_link, *outer_speed);
        return 0;
    }

    outerphy_link[port_index] = 0U;
    outerphy_link[port_index + 4U] = 0U;
    outerphy_link[port_index + 8U] = 0U;
    if (outerphy_link[port_index + 12U] == 0U)
        return 0;

    high_ber_downspeed_check(phy, phy_id, *phy_link, *outer_speed);
    if (sg_linkdn_check_times_mac[port_index] == 2U) {
        phy_zxic_051_apb_write(phy, phy_id, 84U, 1024U);
        register_value = zx_mdio_read_ge_ext_by_port[mdio_slot](phy_id, 0U);
        zx_mdio_write_ge_ext_by_port[mdio_slot](phy_id, 0U,
                                                register_value | 0x0800U);
        --sg_linkdn_check_times_mac[port_index];
        printk("<port:%d> reg0 write 0x1\n", phy);
    } else if (sg_linkdn_check_times_mac[port_index] == 1U) {
        register_value = zx_mdio_read_ge_ext_by_port[mdio_slot](phy_id, 0U);
        zx_mdio_write_ge_ext_by_port[mdio_slot](phy_id, 0U,
                                                register_value & 0xf7ffU);
        --sg_linkdn_check_times_mac[port_index];
        sg_phy_speed_mode_cur_mac[port_index] = 12U;
        sg_phy_speed_mode_last_mac[port_index] = 12U;
        printk("<port:%d> reg0 write 0x0\n", phy);
    }

    if (sg_reg_val_8017_mac[port_index] == 0U) {
        sg_reg_val_8017_mac[port_index] = 1U;
        zx_mdio_write_extended[mdio_slot](phy_id, 1U, 0xffff801eU, 180U);
        zx_mdio_write_extended[mdio_slot](phy_id, 31U, 0xffff80ccU, 127U);
        printk("<port:%d> reg1.8017 write 0x1\n", phy);
    }

    speed_hold_check(phy, phy_id, *phy_link, *outer_speed);
    return 0;
}

void xpcs_set_sr_mii_ctrl_speed(uint8_t xmac, unsigned int speed)
{
    volatile uint32_t *control_register;
    uint32_t control_value;
    uint32_t speed_bits;

    control_register = xpcs_register_for_xmac(xmac, 0x7c0000U);
    control_value = *control_register;
    switch (speed) {
    case 1U:
        speed_bits = 0U;
        break;
    case 2U:
        speed_bits = 0x2000U;
        break;
    case 3U:
        speed_bits = 0x40U;
        break;
    case 4U:
        speed_bits = 0x20U;
        break;
    case 5U:
        speed_bits = 0x2020U;
        break;
    case 6U:
        speed_bits = 0x2040U;
        break;
    default:
        return;
    }

    *control_register = (control_value & 0xffffdf9fU) | speed_bits;
}

void xpcs_set_sr_mii_ctrl_duplex_mode(uint8_t xmac, unsigned int state)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x7c0000U);
    control_value = *control_register;
    *control_register = (control_value & 0xfffffeffU) | ((state & 1U) << 8);
}

void xpcs_set_sr_mii_ctrl_an_enable(uint8_t xmac, unsigned int enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x7c0000U);
    control_value = *control_register;
    *control_register = (control_value & 0xffffefffU) | ((enable & 1U) << 12);
}

void xpcs_set_vr_mii_dig_ctrl1_2_5g_mode_en(uint8_t xmac, uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x7e0000U);
    control_value = *control_register;
    *control_register = (control_value & 0xfffffffbU) |
                        (((uint32_t)enable & 1U) << 2);
}

void xpcs_set_vr_mii_dig_ctrl1_vsmmd1_en(uint8_t xmac, uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x7e0000U);
    control_value = *control_register;
    *control_register = (control_value & 0xffffdfffU) |
                        (((uint32_t)enable & 1U) << 13);
}

void xpcs_set_vr_xs_pcs_dig_ctrl1_2_5g_mode_en(uint8_t xmac,
                                                uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x0e0000U);
    control_value = *control_register;
    *control_register = (control_value & 0xfffffffbU) |
                        (((uint32_t)enable & 1U) << 2);
}

void xpcs_set_vr_xs_pcs_dig_ctrl1_usxg_en(uint8_t xmac, uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x0e0000U);
    control_value = *control_register;
    *control_register = (control_value & 0xfffffdffU) |
                        (((uint32_t)enable & 1U) << 9);
}

void xpcs_set_vr_xs_pcs_dig_ctrl1_vr_rst(uint8_t xmac, uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x0e0000U);
    control_value = *control_register;
    *control_register = (control_value & 0xffff7fffU) |
                        (((uint32_t)enable & 1U) << 15);
}

void xpcs_set_vr_xs_pcs_dig_ctrl1_usra_rst_en(uint8_t xmac,
                                               uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x0e0000U);
    control_value = *control_register;
    *control_register = (control_value & 0xfffffbffU) |
                        (((uint32_t)enable & 1U) << 10);
}

int xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared(uint8_t xmac)
{
    unsigned int retries;

    for (retries = 400U; retries != 0U; --retries) {
        volatile uint32_t *control_register;

        control_register = xpcs_register_for_xmac(xmac, 0x0e0000U);
        if ((*control_register & 0x8000U) == 0U)
            return 0;

        __const_udelay(859000UL);
    }

    return -1;
}

int xpcs_wait_vr_xs_pcs_dig_sts_pseq_state_constprop_0(uint8_t xmac)
{
    unsigned int retries;

    for (retries = 400U; retries != 0U; --retries) {
        volatile uint32_t *status_register;

        status_register = xpcs_register_for_xmac(xmac, 0x0e0040U);
        if (((*status_register >> 2) & 7U) != 4U)
            return 0;
        __const_udelay(859000UL);
    }
    return -1;
}

int xpcs_speed_duplex_conf_in_auto_disable_usxgmii_mode(
    uint8_t xmac, unsigned int speed, unsigned int duplex)
{
    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }

    xpcs_set_sr_mii_ctrl_speed(xmac, speed);
    xpcs_set_sr_mii_ctrl_duplex_mode(xmac, duplex);
    __const_udelay(859000UL);
    xpcs_set_vr_xs_pcs_dig_ctrl1_usra_rst_en(xmac, 1U);
    return xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared(xmac);
}

int xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode(
    uint8_t xmac, unsigned int *uni_speed, unsigned int *duplex,
    unsigned int *auto_status)
{
    volatile uint32_t *interrupt_status_register;
    uint32_t interrupt_status;
    unsigned int reported_speed;
    unsigned int reported_duplex;

    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }

    interrupt_status_register = xpcs_register_for_xmac(xmac, 0x7e0008U);
    interrupt_status = *interrupt_status_register;
    if ((interrupt_status & 1U) == 0U)
        return -1;

    xpcs_clear_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta(xmac);
    if ((interrupt_status & 0x4000U) == 0U) {
        *auto_status = 0U;
        return 0;
    }

    reported_speed = (interrupt_status >> 10) & 7U;
    reported_duplex = (interrupt_status >> 13) & 1U;
    *auto_status = 1U;
    xpcs_switch_vr_mii_an_intr_sts_speed(reported_speed, &reported_speed);
    xpcs_set_sr_mii_ctrl_speed(xmac, reported_speed);
    xpcs_set_sr_mii_ctrl_duplex_mode(xmac, reported_duplex);
    __const_udelay(859000UL);
    xpcs_set_vr_xs_pcs_dig_ctrl1_usra_rst_en(xmac, 1U);
    *uni_speed = reported_speed;
    *duplex = reported_duplex;
    return xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared(xmac);
}

int xpcs_wait_speed_duplex_conf_in_auto_en_usxgmii_mode(uint8_t xmac)
{
    unsigned int uni_speed = 0;
    unsigned int duplex = 0;
    unsigned int auto_status = 0;
    unsigned int retries;
    int status;

    for (retries = 400U; retries != 0U; --retries) {
        status = xpcs_speed_duplex_conf_in_auto_en_usxgmii_mode(
            xmac, &uni_speed, &duplex, &auto_status);
        if (status == 0)
            return status;

        __const_udelay(859000UL);
    }

    printk("in usxgmii mode and auto is enable, pcs in mac side can't get speed and duplex\n");
    return status;
}

void xpcs_set_sr_xs_pcs_ctrl1_low_power_en(uint8_t xmac, uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x0c0000U);
    control_value = *control_register;
    *control_register = (control_value & 0xfffff7ffU) |
                        (((uint32_t)enable & 1U) << 11);
}

void xpcs_set_sr_pma_ctrl1_low_power_en(uint8_t xmac, uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x040000U);
    control_value = *control_register;
    *control_register = (control_value & 0xfffff7ffU) |
                        (((uint32_t)enable & 1U) << 11);
}

void xpcs_set_sr_xs_pcs_ctrl2_pcs_type(uint8_t xmac, unsigned int pcs_type)
{
    volatile uint32_t *type_register;

    type_register = xpcs_register_for_xmac(xmac, 0x0c001cU);
    *type_register = pcs_type;
}

void xpcs_set_vr_xs_pcs_xaui_ctrl_xaui_mode_constprop_1(uint8_t xmac)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x0e0010U);
    control_value = *control_register;
    *control_register = control_value & 0xfffffffeU;
}

void xpcs_eee_cfg(uint8_t xmac, uint8_t profile)
{
    volatile uint32_t *eee_register;

    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return;
    }

    eee_register = xpcs_register_for_xmac(xmac, 0x0e0018U);
    *eee_register = 0x81dfU;
    eee_register = xpcs_register_for_xmac(xmac, 0x0e0020U);
    *eee_register = 0x1cf2U;
    eee_register = xpcs_register_for_xmac(xmac, 0x0e0024U);
    *eee_register = profile == 1U ? 0x2ffaU : 0x35faU;
}

void xpcs_set_sr_xs_pcs_ctrl1_speed_sel_constprop_2(uint8_t xmac)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x0c0000U);
    control_value = *control_register;
    *control_register = control_value & 0xffffdfffU;
}

void xpcs_set_sr_pma_ctrl_speed_sel_constprop_3(uint8_t xmac)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x040000U);
    control_value = *control_register;
    *control_register = control_value & 0xffffdfffU;
}

void xpcs_set_sr_mii_dig_ctrl1_cl37_tmr_ovr_ride(uint8_t xmac,
                                                  uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x7e0000U);
    control_value = *control_register;
    *control_register = (control_value & 0xfffffff7U) |
                        (((uint32_t)enable & 1U) << 3);
}

void xpcs_set_vr_mii_link_timer_ctrl(uint8_t xmac, unsigned int timer)
{
    volatile uint32_t *timer_register;

    timer_register = xpcs_register_for_xmac(xmac, 0x7e0028U);
    *timer_register = timer;
}

void xpcs_set_vr_mii_an_ctrl_tx_config(uint8_t xmac, uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x7e0004U);
    control_value = *control_register;
    *control_register = (control_value & 0xfffffff7U) |
                        (((uint32_t)enable & 1U) << 3);
}

void xpcs_set_vr_mii_an_ctrl_mii_ctrl(uint8_t xmac, uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x7e0004U);
    control_value = *control_register;
    *control_register = (control_value & 0xfffffeffU) |
                        (((uint32_t)enable & 1U) << 8);
}

unsigned int xpcs_get_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta(
    uint8_t xmac, unsigned int *status)
{
    volatile uint32_t *interrupt_status_register;
    unsigned int interrupt_status;

    interrupt_status_register = xpcs_register_for_xmac(xmac, 0x7e0008U);
    interrupt_status = *interrupt_status_register & 1U;
    *status = interrupt_status;
    return interrupt_status;
}

int xpcs_init(uint8_t xmac)
{
    unsigned int retries;
    volatile uint32_t *control_register;

    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }

    for (retries = 400U; retries != 0U; --retries) {
        control_register = xpcs_register_for_xmac(xmac, 0x0c0000U);
        if ((*control_register & 0x8000U) == 0U)
            break;
        __const_udelay(859000UL);
    }
    if (retries == 0U) {
        printk("SR_XS_PCS_CTRL1 soft reset is not completed\n");
        return -1;
    }

    for (retries = 400U; retries != 0U; --retries) {
        control_register = xpcs_register_for_xmac(xmac, 0x7c0000U);
        if ((*control_register & 0x8000U) == 0U)
            break;
        __const_udelay(859000UL);
    }
    if (retries == 0U) {
        printk("SR_MII_CTRL reset is not completed\n");
        return -1;
    }

    xpcs_set_vr_mii_an_ctrl_tx_config(xmac, 0U);
    xpcs_set_vr_mii_an_ctrl_mii_ctrl(xmac, 0U);
    return 0;
}

void xpcs_clear_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta(uint8_t xmac)
{
    volatile uint32_t *interrupt_status_register;
    uint32_t interrupt_status;

    interrupt_status_register = xpcs_register_for_xmac(xmac, 0x7e0008U);
    interrupt_status = *interrupt_status_register;
    *interrupt_status_register = interrupt_status & 0xfffffffeU;
}

unsigned int xpcs_switch_vr_mii_an_intr_sts_speed(
    unsigned int reported_speed, unsigned int *uni_speed)
{
    unsigned int converted_speed;

    switch (reported_speed) {
    case 0U:
        converted_speed = 1U;
        break;
    case 1U:
        converted_speed = 2U;
        break;
    case 2U:
        converted_speed = 3U;
        break;
    case 3U:
        converted_speed = 6U;
        break;
    case 4U:
        converted_speed = 4U;
        break;
    case 5U:
        converted_speed = 5U;
        break;
    default:
        converted_speed = 7U;
        break;
    }

    *uni_speed = converted_speed;
    return converted_speed;
}

int xpcs_get_speed_duplex_in_auto_en_sgmii_mode(
    uint8_t xmac, unsigned int *uni_speed, unsigned int *duplex,
    unsigned int *auto_status)
{
    volatile uint32_t *interrupt_status_register;
    uint32_t interrupt_status;
    unsigned int reported_speed;

    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }

    interrupt_status_register = xpcs_register_for_xmac(xmac, 0x7e0008U);
    interrupt_status = *interrupt_status_register;
    if ((interrupt_status & 1U) == 0U)
        return -1;

    xpcs_clear_vr_mii_an_intr_sts_cl37_ancmplt_intr_sta(xmac);
    if ((interrupt_status & 0x10U) == 0U) {
        *auto_status = 0U;
        return 0;
    }

    reported_speed = (interrupt_status >> 2) & 3U;
    *auto_status = 1U;
    xpcs_switch_vr_mii_an_intr_sts_speed(reported_speed, &reported_speed);
    *uni_speed = reported_speed;
    *duplex = (interrupt_status >> 1) & 1U;
    return 0;
}

void xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en(uint8_t xmac, uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x0e0000U);
    control_value = *control_register;
    *control_register = (control_value & 0xffffdfffU) |
                        (((uint32_t)enable & 1U) << 13);
}

unsigned int xpcs_sr_mii_ctrl_is_an_enable(uint8_t xmac)
{
    volatile uint32_t *control_register;

    control_register = xpcs_register_for_xmac(xmac, 0x7c0000U);
    return (*control_register >> 12) & 1U;
}

void xpcs_set_vr_mii_an_ctrl_sgmii_link_sts(uint8_t xmac,
                                             unsigned int link_status)
{
    volatile uint32_t *link_status_register;
    uint32_t link_status_value;

    link_status_register = xpcs_register_for_xmac(xmac, 0x7e0004U);
    link_status_value = *link_status_register;
    *link_status_register = (link_status_value & 0xffffffefU) |
                            ((link_status & 1U) << 4);
}

int xpcs_auto_negotiation_conf_in_sgmii_mode(uint8_t xmac,
                                              uint8_t auto_enable)
{
    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }

    if (sg_xpcs_mode[xmac] != 3)
        return -1;

    xpcs_set_sr_mii_ctrl_an_enable(xmac, 0U);
    xpcs_set_vr_mii_an_ctrl_an_intr_en(xmac, auto_enable);
    xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw(xmac, auto_enable);
    if (auto_enable == 1U) {
        xpcs_set_sr_mii_ctrl_an_enable(xmac, auto_enable);
        g_xmac_work_in_auto[xmac] = auto_enable;
    } else {
        xpcs_set_vr_mii_an_ctrl_sgmii_link_sts(xmac, 1U);
    }

    return 0;
}

void xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw(uint8_t xmac, uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x7e0000U);
    control_value = *control_register;
    *control_register = (control_value & 0xfffffdffU) |
                        (((uint32_t)enable & 1U) << 9);
}

void xpcs_set_vr_mii_an_ctrl_an_intr_en(uint8_t xmac, uint8_t enable)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x7e0004U);
    control_value = *control_register;
    *control_register = (control_value & 0xfffffffeU) |
                        ((uint32_t)enable & 1U);
}

int xpcs_auto_negotiation_conf_in_usxgmii_mode(uint8_t xmac,
                                                uint8_t auto_enable)
{
    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }

    xpcs_set_vr_mii_an_ctrl_an_intr_en(xmac, auto_enable);
    xpcs_set_sr_mii_ctrl_an_enable(xmac, auto_enable);
    if (auto_enable == 1U)
        g_xmac_work_in_auto[xmac] = auto_enable;

    return 0;
}

void xpcs_set_vr_mii_an_ctrl_pcs_mode(uint8_t xmac, unsigned int pcs_mode)
{
    volatile uint32_t *control_register;
    uint32_t control_value;

    control_register = xpcs_register_for_xmac(xmac, 0x7e0004U);
    control_value = *control_register;
    *control_register = (control_value & 0xfffffff9U) |
                        ((pcs_mode & 3U) << 1);
}

void xpcs_prepare_for_switch_mode(uint8_t xmac, unsigned int target_mode)
{
    int32_t current_mode;

    g_xmac_work_in_auto[xmac] = 0U;
    current_mode = sg_xpcs_mode[xmac];
    if ((unsigned int)current_mode == target_mode)
        return;

    switch (current_mode) {
    case 3:
        xpcs_exit_sgmii_mode(xmac);
        break;
    case 5:
    case 6:
    case 7:
        xpcs_exit_usxgmii_mode(xmac);
        break;
    case 8:
        xpcs_exit_hsgmii_mode(xmac);
        break;
    default:
        break;
    }
}

void xpcs_exit_sgmii_mode(uint8_t xmac)
{
    xpcs_set_sr_mii_ctrl_an_enable(xmac, 0U);
    xpcs_set_vr_mii_an_ctrl_an_intr_en(xmac, 0U);
    xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw(xmac, 0U);
    xpcs_set_vr_xs_pcs_dig_ctrl1_2_5g_mode_en(xmac, 0U);
    xpcs_set_vr_mii_dig_ctrl1_2_5g_mode_en(xmac, 0U);
    xpcs_set_vr_mii_an_ctrl_sgmii_link_sts(xmac, 0U);
}

void xpcs_exit_usxgmii_mode(uint8_t xmac)
{
    xpcs_set_vr_xs_pcs_dig_ctrl1_usxg_en(xmac, 0U);
    xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en(xmac, 1U);
}

void xpcs_exit_hsgmii_mode(uint8_t xmac)
{
    xpcs_set_vr_mii_dig_ctrl1_2_5g_mode_en(xmac, 0U);
    xpcs_set_vr_mii_dig_ctrl1_vsmmd1_en(xmac, 0U);
    xpcs_set_vr_xs_pcs_dig_ctrl1_2_5g_mode_en(xmac, 0U);
    xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en(xmac, 0U);
}

int xpcs_usxgmii_mode_conf(uint8_t xmac, unsigned int usxg_mode)
{
    volatile uint32_t *mode_register;
    uint32_t mode_value;
    int32_t cached_mode;

    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }

    xpcs_prepare_for_switch_mode(xmac, 5U);
    xpcs_set_sr_xs_pcs_ctrl2_pcs_type(xmac, 0U);
    xpcs_set_vr_xs_pcs_dig_ctrl1_usxg_en(xmac, 1U);
    xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en(xmac, 1U);

    mode_register = xpcs_register_for_xmac(xmac, 0x0e001cU);
    mode_value = *mode_register;
    *mode_register = (mode_value & 0xffffe3ffU) |
                     ((usxg_mode & 7U) << 10);
    xpcs_set_vr_xs_pcs_dig_ctrl1_vr_rst(xmac, 1U);
    xpcs_wait_vr_xs_pcs_dig_ctrl1_vr_rst_cleared(xmac);

    switch (usxg_mode) {
    case 0U:
    case 3U:
        cached_mode = 5;
        break;
    case 1U:
    case 4U:
        cached_mode = 6;
        break;
    case 2U:
    case 5U:
        cached_mode = 7;
        break;
    default:
        printk("unsupported usxg_mode(%d)\n", usxg_mode);
        return -1;
    }

    sg_xpcs_mode[xmac] = cached_mode;
    return 0;
}

int xpcs_1g_mode_conf(uint8_t xmac, unsigned int speed,
                      unsigned int duplex, unsigned int pcs_mode)
{
    xpcs_set_sr_xs_pcs_ctrl2_pcs_type(xmac, 1U);
    xpcs_set_vr_xs_pcs_xaui_ctrl_xaui_mode_constprop_1(xmac);
    xpcs_set_sr_pma_ctrl_speed_sel_constprop_3(xmac);
    xpcs_set_sr_xs_pcs_ctrl1_speed_sel_constprop_2(xmac);
    xpcs_set_sr_mii_ctrl_speed(xmac, speed);
    xpcs_set_sr_mii_ctrl_duplex_mode(xmac, duplex);
    xpcs_set_sr_xs_pcs_ctrl1_low_power_en(xmac, 1U);
    if (xpcs_wait_vr_xs_pcs_dig_sts_pseq_state_constprop_0(xmac) != 0)
        return -1;

    xpcs_set_sr_xs_pcs_ctrl1_low_power_en(xmac, 0U);
    xpcs_set_vr_mii_an_ctrl_pcs_mode(xmac, pcs_mode);
    return 0;
}

int xpcs_2p5gbase_x_conf(uint8_t xmac)
{
    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }

    xpcs_prepare_for_switch_mode(xmac, 4U);
    xpcs_set_sr_xs_pcs_ctrl2_pcs_type(xmac, 0x0eU);
    xpcs_set_sr_pma_ctrl_speed_sel_constprop_3(xmac);
    xpcs_set_sr_pma_ctrl1_low_power_en(xmac, 1U);
    __const_udelay(859000UL);
    xpcs_set_sr_pma_ctrl1_low_power_en(xmac, 0U);
    sg_xpcs_mode[xmac] = 4;
    return 0;
}

int xpcs_10gbase_r_conf(uint8_t xmac)
{
    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }

    xpcs_prepare_for_switch_mode(xmac, 0U);
    xpcs_set_sr_xs_pcs_ctrl2_pcs_type(xmac, 0U);
    xpcs_set_sr_xs_pcs_ctrl1_low_power_en(xmac, 1U);
    __const_udelay(859000UL);
    xpcs_set_sr_xs_pcs_ctrl1_low_power_en(xmac, 0U);
    sg_xpcs_mode[xmac] = 0;
    return 0;
}

int xpcs_5gbase_r_conf(uint8_t xmac)
{
    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }

    xpcs_prepare_for_switch_mode(xmac, 1U);
    xpcs_set_sr_xs_pcs_ctrl2_pcs_type(xmac, 0x0fU);
    xpcs_set_sr_xs_pcs_ctrl1_low_power_en(xmac, 1U);
    __const_udelay(859000UL);
    xpcs_set_sr_xs_pcs_ctrl1_low_power_en(xmac, 0U);
    sg_xpcs_mode[xmac] = 1;
    return 0;
}

int xpcs_1000base_x_conf(uint8_t xmac, unsigned int speed,
                          unsigned int duplex)
{
    int status;

    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }
    xpcs_prepare_for_switch_mode(xmac, 2U);
    status = xpcs_1g_mode_conf(xmac, speed, duplex, 0U);
    sg_xpcs_mode[xmac] = 2;
    return status;
}

int xpcs_sgmii_mode_conf(uint8_t xmac, unsigned int mode_value,
                          unsigned int config_value)
{
    int status;

    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }
    xpcs_prepare_for_switch_mode(xmac, 3U);
    status = xpcs_1g_mode_conf(xmac, mode_value, config_value, 2U);
    sg_xpcs_mode[xmac] = 3;
    return status;
}

int xpcs_hsgmii_mode_conf(uint8_t xmac, uint8_t auto_enable)
{
    int status;

    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }
    if (isCpuType_133() == 1 || isCpuType_129() == 1)
        xpcs_prepare_for_switch_mode(xmac, 8U);
    xpcs_set_sr_xs_pcs_ctrl2_pcs_type(xmac, 1U);
    xpcs_set_vr_xs_pcs_xaui_ctrl_xaui_mode_constprop_1(xmac);
    xpcs_set_sr_pma_ctrl_speed_sel_constprop_3(xmac);
    xpcs_set_sr_xs_pcs_ctrl1_speed_sel_constprop_2(xmac);
    xpcs_set_vr_mii_dig_ctrl1_2_5g_mode_en(xmac, 1U);
    xpcs_set_vr_mii_dig_ctrl1_vsmmd1_en(xmac, 1U);
    xpcs_set_vr_xs_pcs_dig_ctrl1_2_5g_mode_en(xmac, 1U);
    xpcs_set_vr_xs_pcs_dig_ctrl1_vsmmd1_en(xmac, 1U);
    xpcs_set_sr_mii_ctrl_speed(xmac, 3U);
    xpcs_set_sr_mii_ctrl_duplex_mode(xmac, 1U);
    xpcs_set_sr_xs_pcs_ctrl1_low_power_en(xmac, 1U);
    status = xpcs_wait_vr_xs_pcs_dig_sts_pseq_state_constprop_0(xmac);
    if (status != 0)
        return -1;
    xpcs_set_sr_xs_pcs_ctrl1_low_power_en(xmac, 0U);
    xpcs_set_vr_mii_an_ctrl_pcs_mode(xmac, 2U);
    xpcs_set_sr_mii_ctrl_an_enable(xmac, 0U);
    xpcs_set_vr_mii_an_ctrl_an_intr_en(xmac, auto_enable);
    xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw(xmac, auto_enable);
    xpcs_set_vr_mii_link_timer_ctrl(xmac, 1953U);
    xpcs_set_sr_mii_dig_ctrl1_cl37_tmr_ovr_ride(xmac, 1U);
    if (isCpuType_133() == 1 || isCpuType_129() == 1) {
        if (auto_enable == 1U) {
            xpcs_set_sr_mii_ctrl_an_enable(xmac, 1U);
            g_xmac_work_in_auto[xmac] = 1U;
        } else {
            xpcs_set_vr_mii_an_ctrl_sgmii_link_sts(xmac, 1U);
        }
    }
    sg_xpcs_mode[xmac] = 8;
    return status;
}

int xpcs_auto_negotiation_conf_in_1000base_x_mode(
    uint8_t xmac, uint8_t auto_enable, uint8_t enable_2p5g)
{
    if (xmac > 4U) {
        printk("xmac_index(%d) is error\n", xmac);
        return -1;
    }
    if (sg_xpcs_mode[xmac] != 3)
        return -1;
    xpcs_set_sr_mii_ctrl_an_enable(xmac, 0U);
    xpcs_set_vr_mii_an_ctrl_an_intr_en(xmac, auto_enable);
    xpcs_set_vr_mii_dig_ctrl1_mac_auto_sw(xmac, auto_enable);
    xpcs_set_vr_xs_pcs_dig_ctrl1_2_5g_mode_en(xmac, enable_2p5g);
    xpcs_set_vr_mii_dig_ctrl1_2_5g_mode_en(xmac, enable_2p5g);
    if (enable_2p5g == 1U) {
        xpcs_set_vr_mii_link_timer_ctrl(xmac, 1953U);
        xpcs_set_sr_mii_dig_ctrl1_cl37_tmr_ovr_ride(xmac, enable_2p5g);
    }
    if (auto_enable == 1U)
        xpcs_set_sr_mii_ctrl_an_enable(xmac, auto_enable);
    return 0;
}

void phy_051_set_xmac_speed(uint8_t xmac, unsigned int uni_speed,
                             unsigned int pcs_mode)
{
    unsigned int xmac_speed = 0;

    if (uni_speed > 6U)
        return;

    if (pcs_mode == 3U) {
        xmac_switch_uni_speed_to_xmac_speed(xmac, uni_speed, &xmac_speed);
        xmac_set_speed_sel(xmac, xmac_speed);
        printk("change xmac %u speed 0x%x in mode 0x%x\n", xmac,
               uni_speed, pcs_mode);
    } else if (pcs_mode == 6U) {
        xmac_speed_process_in_sgmii_auto_mode(xmac);
    }
}

int phy_051_set_xmac_work_mode(uint8_t xmac, unsigned int pcs_mode)
{
    int status = 0;

    if (pcs_mode == 3U)
        return xmac_2pt5gbase_x_conf(xmac);

    if (pcs_mode == 6U) {
        status = xmac_sgmii_conf(xmac, 1U, 3U, 1U);
        xmac_set_speed_sel(xmac, 1U);
    }

    return status;
}

int phy_zxic051_check(uint8_t phy_id)
{
    unsigned int phy_state = 0;
    int nppt_link = 0;
    int xmac_speed = 0;
    uint8_t outer_speed = 0;
    uint8_t duplex = 0;
    uint8_t phy_link = 0;
    unsigned int xmac = byte_266BC != phy_id;
    unsigned int pcs_mode;

    phy_zxic_051_phy_uni_check(xmac + 4U, &phy_state, &phy_link,
                                &outer_speed, &duplex);
    pcs_mode = sg_phy_speed_mode_cur_mac[xmac];
    phy_zxic_speed_outer2uni(&outer_speed);

    if (pcs_mode != sg_last_serdes_mode0_54532[xmac]) {
        uint32_t previous_mode = sg_last_serdes_mode0_54532[xmac];

        printk("cur %u phy  serdes_mode : 0x%x, last serdes_mode0 : 0x%x, last serdes_mode1 : 0x%x\n",
               xmac, pcs_mode, previous_mode,
               sg_last_serdes_mode1_54533[xmac]);
        if (pcs_mode != 12U &&
            (previous_mode != 12U ||
             pcs_mode != sg_last_serdes_mode1_54533[xmac]) &&
            (isCpuType_133() == 1 || isCpuType_129() == 1)) {
            xmac_mode_set((uint8_t)xmac, pcs_mode, outer_speed, duplex);
            printk("xmac_mode_set 1 xmac_id %u, serdes_mode %u, speed %u, duplex %u\n",
                   xmac, pcs_mode, outer_speed, duplex);
        }
        sg_last_serdes_mode1_54533[xmac] = previous_mode;
        sg_last_serdes_mode0_54532[xmac] = pcs_mode;
    }

    if (phy_link == 0)
        return -1;

    xmac_get_nppt_glb_link_status((uint8_t)xmac, &nppt_link);
    if (nppt_link == 0) {
        ++sg_051_interval_cnt_54535[xmac];
        if (sg_051_interval_cnt_54535[xmac] !=
            phy_check_reset_serdes_interval_num)
            return -1;

        if (isCpuType_133() == 1 || isCpuType_129() == 1) {
            xmac_mode_set((uint8_t)xmac, pcs_mode, outer_speed, duplex);
            printk("xmac_mode_set 2 xmac_id %u, serdes_mode %u, speed %u, duplex %u\n",
                   xmac, pcs_mode, outer_speed, duplex);
        }
        sg_051_interval_cnt_54535[xmac] = 0;
        return -1;
    }

    xmac_speed_process((uint8_t)xmac);
    xmac_get_uni_speed_from_xmac((uint8_t)xmac, &xmac_speed);
    sg_051_interval_cnt_54535[xmac] = 0;
    if (outer_speed != xmac_speed) {
        phy_051_set_xmac_speed((uint8_t)xmac, outer_speed, pcs_mode);
        ++sg_051_re_an_cnt_54536[xmac];
        if (sg_051_re_an_cnt_54536[xmac] !=
            phy_check_reset_serdes_interval_num)
            return -1;

        if (isCpuType_133() == 1 || isCpuType_129() == 1) {
            xmac_mode_set((uint8_t)xmac, pcs_mode, outer_speed, duplex);
            printk("xmac_mode_set 2 xmac_id %u, serdes_mode %u, speed %u, duplex %u\n",
                   xmac, pcs_mode, outer_speed, duplex);
        }
        sg_051_re_an_cnt_54536[xmac] = 0;
        return -1;
    }

    if (sg_051_re_an_cnt_54536[xmac] != 0)
        sg_051_re_an_cnt_54536[xmac] = 0;
    return (int)outer_speed + ((duplex & 1U) << 10);
}

void smac_del_phy_scan(void)
{
    del_timer(&phy_timer);
}

void nppt_smac_set_rgmii_mode(void)
{
    uint32_t value = NPPT_U32(0x24U);

    NPPT_U32(0x24U) = (value & 0xfc7fffffU) | 0x800000U;
    pon_soc_pon_rgmii_clk_set(0);
    greg_rgmii_intf_mode_set(1U);
    printk("nppt_smac_set_rgmii_mode (0x%x) finish !\n", 0x800000U);
}
