/*
 * Semantic reconstruction of zx_pon_probe at 0x0580.
 *
 * This is source-like C, not a buildable vendor replacement. Types and helper
 * declarations are deliberately minimal. Names and control flow come from the
 * vendor module and vendor runtime/DTS evidence, not from linux-6.18.38.
 */

#include <stdint.h>

typedef struct {
    const char *name; /* Observed prefix only; not a full vendor ABI claim. */
} zte_platform_device_prefix_t;

typedef void zte_of_node_t;

extern const void *zx_pon_match;

extern zte_of_node_t *of_find_matching_node_and_match(
    zte_of_node_t *from, const void *matches, const void *match_out);
extern int of_device_is_compatible(const zte_of_node_t *node,
                                   const char *compatible);
extern void *of_iomap(const zte_of_node_t *node, unsigned int index);
extern int of_irq_get(const zte_of_node_t *node, unsigned int index);
extern int arm64_kernel_use_ng_mappings(void);
extern void *__ioremap(uintptr_t physical, unsigned long size,
                       unsigned long flags);

extern int printk(const char *format, ...);
extern int isCpuType_133(void);
extern int isCpuType_129(void);
extern int CspGetPortInfo(uint16_t **mode);
extern void nppt_idm_cci_enable(void);
extern void pon_soc_pon_cci_aclk_init(void);
extern void pon_soc_pon_woe_clk_init(void);
extern void pon_soc_pon_core_clk_init(void);
extern void pon_soc_pon_tm_aclk_init(void);
extern void pon_soc_pon_nppt_clk_init(void);
extern void greg_sdet_to_reset(void);
extern void greg_sdet_to_restore(void);
extern void __const_udelay(unsigned long cycles);
extern void pon_sys_soft_reset(void);
extern int sipc_init(void);
extern int greg_init_done_check(void);
extern void zx_pon_clk_reset_init(unsigned int mode);
extern unsigned int get_eth_wan_port(void);
extern int IsXmac1Used(void);
extern void ponserdes_to_xmac1_en_set(int enable);
extern int register_pon_int(void);
extern int register_nppt_int(void);
extern void unregister_pon_int(void);
extern void unregister_nppt_int(void);

extern void *pon_base;
extern void *sys_ctrl_base;
extern void *top_crm_base;
extern void *pin_mux_base;
extern void *efuse_base;
extern void *pps_base;
extern void *nppt_base;
extern void *rgmii_base;
extern void *xmac0_pcs_base;
extern void *pon_serdes_base;
extern void *pon_serdes_pll_base;
extern void *uni_serdes_base;
extern void *pcu_base;
extern void *gephy_apb_base;
extern void *lowpower_config_base;
extern void *lowpower_xmac_config_base;
extern void *lowpower_gpio_config_base;

extern int g_pon_irq;
extern int g_pps_irq;
extern int g_nppt_irq;
extern int g_woe0_tx_int;
extern int g_woe0_rx_int;
extern int g_woe1_tx_int;
extern int g_woe1_rx_int;
extern int g_idm_irq[4];
extern uint16_t g_pon_work_mode;
extern uint16_t g_pon_work_mode_orignal;
extern int lan_up;
extern int lan_up_port;

#define PON_PROBE_RESOURCE_ERROR (-19)

#define PON_MODE_P2P    0x010U
#define PON_MODE_EPON   0x020U
#define PON_MODE_GPON   0x040U
#define PON_MODE_XEPON  0x080U
#define PON_MODE_XEPONS 0x100U
#define PON_MODE_XGPON  0x200U
#define PON_MODE_XGPONS 0x400U

/* The original repeats this flags selection for each manual alias mapping. */
#define VENDOR_IOREMAP_FLAGS \
    (arm64_kernel_use_ng_mappings() ? 0x68000000000f07ULL : 0x68000000000707ULL)

int zx_pon_probe_recovered(zte_platform_device_prefix_t *pdev)
{
    zte_of_node_t *node;
    uint16_t *port_mode;
    int ret;
    unsigned int index;

    printk("pon init\n");

    for (node = of_find_matching_node_and_match(0, zx_pon_match, 0);
         node != 0;
         node = of_find_matching_node_and_match(node, zx_pon_match, 0)) {
        if (!isCpuType_133() && !isCpuType_129())
            continue;

        if (of_device_is_compatible(node, "zte,zx279133-pon")) {
            pon_base = of_iomap(node, 0);
            if (!pon_base)
                goto resource_error;
            sys_ctrl_base = of_iomap(node, 1);
            if (!sys_ctrl_base)
                goto resource_error;
            top_crm_base = of_iomap(node, 2);
            if (!top_crm_base)
                goto resource_error;
            pin_mux_base = of_iomap(node, 3);
            if (!pin_mux_base)
                goto resource_error;
            efuse_base = of_iomap(node, 4);
            if (!efuse_base)
                goto resource_error;

            g_pon_irq = of_irq_get(node, 0);
            if (g_pon_irq <= 0)
                goto resource_error;
            g_woe0_tx_int = of_irq_get(node, 1);
            g_woe0_rx_int = of_irq_get(node, 2);
            g_woe1_tx_int = of_irq_get(node, 3);
            g_woe1_rx_int = of_irq_get(node, 4);
        }

        if (of_device_is_compatible(node, "zte,zx279133-pps")) {
            pps_base = of_iomap(node, 0);
            if (!pps_base)
                goto resource_error;
            g_pps_irq = of_irq_get(node, 0);
            if (g_pps_irq <= 0)
                goto resource_error;
        }

        if (of_device_is_compatible(node, "zte,zx279133-nppt")) {
            nppt_base = of_iomap(node, 0);
            if (!nppt_base)
                goto resource_error;
            g_nppt_irq = of_irq_get(node, 0);
            if (g_nppt_irq <= 0)
                goto resource_error;
        }

        if (of_device_is_compatible(node, "zte,zx279133-rgmii")) {
            rgmii_base = of_iomap(node, 0);
            if (!rgmii_base)
                goto resource_error;
        }

        if (of_device_is_compatible(node, "zte,zx279133-idm-intr")) {
            for (index = 0; index < 4; ++index) {
                g_idm_irq[index] = of_irq_get(node, index);
                if (g_idm_irq[index] <= 0)
                    goto resource_error;
            }
        }

        if (of_device_is_compatible(node, "zte,zx279133-xmac0-pcs")) {
            xmac0_pcs_base = of_iomap(node, 0);
            if (!xmac0_pcs_base)
                goto resource_error;
        }
        if (of_device_is_compatible(node, "zte,zx279133-pon_serdes")) {
            pon_serdes_base = of_iomap(node, 0);
            if (!pon_serdes_base)
                goto resource_error;
        }
        if (of_device_is_compatible(node, "zte,zx279133-pon_serdes_pll")) {
            pon_serdes_pll_base = of_iomap(node, 0);
            if (!pon_serdes_pll_base)
                goto resource_error;
        }
        if (of_device_is_compatible(node, "zte,zx279133-uni_serdes")) {
            uni_serdes_base = of_iomap(node, 0);
            if (!uni_serdes_base)
                goto resource_error;
        }
        if (of_device_is_compatible(node, "zte,zx279133-pcu")) {
            pcu_base = of_iomap(node, 0);
            if (!pcu_base)
                goto resource_error;
        }
        if (of_device_is_compatible(node, "zte,zx279133-gephy-apb")) {
            gephy_apb_base = of_iomap(node, 0);
            if (!gephy_apb_base)
                goto resource_error;
        }

        /* These aliases are intentionally inside the original OF-node loop. */
        lowpower_config_base = __ioremap(0x10e10000U, 0x200U,
                                         VENDOR_IOREMAP_FLAGS);
        lowpower_xmac_config_base = __ioremap(0x16100000U, 0x200U,
                                              VENDOR_IOREMAP_FLAGS);
        lowpower_gpio_config_base = __ioremap(0x10e20000U, 0x200U,
                                              VENDOR_IOREMAP_FLAGS);
    }

    if (CspGetPortInfo(&port_mode) == 0) {
        g_pon_work_mode = *port_mode;
        g_pon_work_mode_orignal = *port_mode;
    }

    printk("pon init %s,pon mode %x\n", pdev->name, g_pon_work_mode);

    if (isCpuType_133() || isCpuType_129()) {
        nppt_idm_cci_enable();
        pon_soc_pon_cci_aclk_init();
        pon_soc_pon_woe_clk_init();
        pon_soc_pon_core_clk_init();
        pon_soc_pon_tm_aclk_init();
        pon_soc_pon_nppt_clk_init();
    }

    greg_sdet_to_reset();
    __const_udelay(1718000U);
    pon_sys_soft_reset();
    sipc_init();
    ret = greg_init_done_check();
    if (ret != 0)
        return ret;

    greg_sdet_to_restore();
    if (g_pon_work_mode & PON_MODE_GPON) {
        zx_pon_clk_reset_init(5);
    } else if (g_pon_work_mode & PON_MODE_XGPON) {
        zx_pon_clk_reset_init(6);
    } else if (g_pon_work_mode & PON_MODE_XGPONS) {
        zx_pon_clk_reset_init(7);
    } else if (g_pon_work_mode & PON_MODE_EPON) {
        zx_pon_clk_reset_init(0);
    } else if (g_pon_work_mode & PON_MODE_XEPON) {
        zx_pon_clk_reset_init(1);
    } else if (g_pon_work_mode & PON_MODE_XEPONS) {
        zx_pon_clk_reset_init(4);
    } else if (g_pon_work_mode & PON_MODE_P2P) {
        zx_pon_clk_reset_init(15);
        lan_up_port = 0;
        lan_up = 1;
    } else {
        zx_pon_clk_reset_init(7);
    }

    if (isCpuType_133() || isCpuType_129()) {
        volatile uint32_t *sysctrl = (volatile uint32_t *)sys_ctrl_base;
        volatile uint32_t *efuse = (volatile uint32_t *)efuse_base;

        lan_up_port = (int)get_eth_wan_port() + 1;
        ponserdes_to_xmac1_en_set(lan_up == 1 && IsXmac1Used());
        sysctrl[4] = (sysctrl[4] & 0xffffff0fU) |
                     (16U * ((efuse[17] >> 26) & 0x0fU));
    }

    ret = register_pon_int();
    if (ret < 0)
        return ret;
    ret = register_nppt_int();
    if (ret < 0)
        return ret;

    printk("pon probe init ok\n");
    return 0;

resource_error:
    /* Original code logs a resource-specific message, then this common text. */
    printk("get pon irq fail!\n");
    return PON_PROBE_RESOURCE_ERROR;
}

int zx_pon_remove_recovered(zte_platform_device_prefix_t *pdev)
{
    (void)pdev;

    unregister_pon_int();
    unregister_nppt_int();
    return 0;
}
