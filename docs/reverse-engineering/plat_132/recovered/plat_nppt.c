/* Semantic reconstruction of module-local NPPT control helpers. */

#include <stdint.h>

#define NPPT_U32(offset) (*(volatile uint32_t *)(nppt_base + (offset)))

extern int printk(const char *format, ...);
extern void __const_udelay(unsigned long loops);
extern void idm_exit(void);
extern void smac_del_phy_scan(void);
extern void queued_spin_lock_slowpath(uint32_t *lock, uint32_t observed,
                                       uintptr_t argument_2,
                                       unsigned int argument_3);
extern uint64_t read_tpidr_el2(void);
extern uint64_t read_icc_pmr_el1(void);
extern void write_icc_pmr_el1(uint64_t value);
extern void __dsb(unsigned int option);
extern unsigned long read_daif(void);
extern void daifset_irq(void);
extern void write_daif(unsigned long flags);
extern int sipc_init(void);
extern volatile uint8_t *nppt_base;
extern uint32_t uNORMAL_BP_SIZE;
extern uint32_t uJUMBO_BP_SIZE;
extern uint32_t uSKB_SHAREDINFO_SIZE;
extern uint32_t uBP_BUFFER_OFFSET;
extern volatile uint32_t nppt_glb_auto_gate_lock;

int sub_165A0(void)
{
    uint64_t priority_mask;

    (void)read_tpidr_el2();
    priority_mask = read_icc_pmr_el1();
    write_icc_pmr_el1((uint32_t)priority_mask ^ 0xe0U);
    write_icc_pmr_el1(priority_mask);
    __dsb(0x0fU);
    return sipc_init();
}

int sipc_init(void)
{
    NPPT_U32(0x4000U) = 0;
    return 0;
}

int soam_init(void)
{
    NPPT_U32(0x2c0000U) = NPPT_U32(0x2c0000U) | 3U;
    while ((NPPT_U32(0x80U) & 2U) == 0)
        ;

    printk("soam init done!\n");
    return 0;
}

int nppt_nppu_reset(void)
{
    uint32_t original = NPPT_U32(0x2c0004U);
    uint32_t reset_value = original & ~0x80U;
    uint32_t restored_value;

    printk("val =0x%x, reg = 0x%px\n", original,
           (const void *)(nppt_base + 0x2c0004U));
    NPPT_U32(0x2c0004U) = reset_value;
    printk("reset val =0x%x\n", reset_value);
    __const_udelay(1718000UL);
    restored_value = reset_value | 0x80U;
    NPPT_U32(0x2c0004U) = restored_value;
    printk("nppt_nppu_reset restore val = 0x%x\n", restored_value);
    return 0;
}

int nppt_tm_reset(void)
{
    uint32_t original = NPPT_U32(0x2c0004U);
    uint32_t reset_value = original & ~0x100U;
    uint32_t restored_value;

    printk("val =0x%x, reg = 0x%px\n", original,
           (const void *)(nppt_base + 0x2c0004U));
    NPPT_U32(0x2c0004U) = reset_value;
    printk("reset val =0x%x\n", reset_value);
    __const_udelay(1718000UL);
    restored_value = reset_value | 0x100U;
    NPPT_U32(0x2c0004U) = restored_value;
    printk("nppt_tm_reset restore val = 0x%x\n", restored_value);
    return 0;
}

void nppt_exit(void)
{
    idm_exit();
    smac_del_phy_scan();
}

unsigned long arch_local_irq_save_0(void)
{
    unsigned long flags = read_daif();

    if ((flags & 0x80U) == 0)
        daifset_irq();
    return flags;
}

void arch_local_irq_restore_1(unsigned long flags)
{
    write_daif(flags);
}

void greg_sdet_to_reset(void)
{
    uint32_t original = NPPT_U32(0x2c0004U);
    uint32_t reset_value = original & ~0x10U;

    printk("val =0x%x, reg = 0x%px\n", original,
           (const void *)(nppt_base + 0x2c0004U));
    NPPT_U32(0x2c0004U) = reset_value;
    printk("reset val =0x%x\n", reset_value);
}

int greg_init_done_check(void)
{
    unsigned int count = 0;

    for (;;) {
        if ((NPPT_U32(0x80U) & 0x1fdU) == 0x1fdU) {
            printk("nppt init done ok. cnt = %u\n", count);
            return 0;
        }

        ++count;
        __const_udelay(429500UL);
        if (count == 400U)
            break;
    }

    printk("nppt init done fail\n");
    return -1;
}

void greg_sdet_to_restore(void)
{
    uint32_t original = NPPT_U32(0x2c0004U);
    uint32_t restored_value = original | 0x10U;

    printk("val =0x%x, reg = 0x%px\n", original,
           (const void *)(nppt_base + 0x2c0004U));
    __const_udelay(859000UL);
    NPPT_U32(0x2c0004U) = restored_value;
    printk("restore val = 0x%x\n", restored_value);
}

static void do_raw_spin_lock_flags_isra_1_constprop_3(void)
{
    __builtin_prefetch((const void *)&nppt_glb_auto_gate_lock, 1, 1);
    for (;;) {
        uint32_t observed = __atomic_load_n(&nppt_glb_auto_gate_lock,
                                            __ATOMIC_ACQUIRE);
        uint32_t expected = 0;

        if (observed != 0) {
            queued_spin_lock_slowpath((uint32_t *)&nppt_glb_auto_gate_lock,
                                      observed, 0, 1U);
            return;
        }
        if (__atomic_compare_exchange_n(&nppt_glb_auto_gate_lock, &expected, 1U,
                                        0, __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED))
            return;
    }
}

uint32_t greg_sopc_auto_gate_en_get(void)
{
    unsigned long flags = arch_local_irq_save_0();
    uint32_t value;
    uint32_t enabled;

    do_raw_spin_lock_flags_isra_1_constprop_3();
    value = NPPT_U32(0xb8U);
    enabled = (value >> 4) & 1U;
    __atomic_store_n((uint8_t *)&nppt_glb_auto_gate_lock, 0,
                     __ATOMIC_RELEASE);
    arch_local_irq_restore_1(flags);
    printk("nppt gate is 0x%x, en is 0x%x\n", value, enabled);
    return enabled;
}

void greg_sopc_auto_gate_en_set(uint32_t enable)
{
    unsigned long flags = arch_local_irq_save_0();
    uint32_t value;

    do_raw_spin_lock_flags_isra_1_constprop_3();
    value = (NPPT_U32(0xb8U) & ~0x10U) | ((enable & 1U) << 4);
    NPPT_U32(0xb8U) = value;
    __atomic_store_n((uint8_t *)&nppt_glb_auto_gate_lock, 0,
                     __ATOMIC_RELEASE);
    arch_local_irq_restore_1(flags);
    printk("set nppt gate = 0x%x\n", value);
}

void greg_smac0_3_mask_runt_err(uint32_t mac)
{
    uint32_t offset = ((mac + 13U) & 0x3fffffffU) << 2;

    NPPT_U32(offset) = NPPT_U32(offset) | 0x40000U;
}

void greg_smac6_mask_runt_err(void)
{
    NPPT_U32(0x4cU) = NPPT_U32(0x4cU) | 0x10000U;
}

void greg_xmac_mask_runt_type(uint32_t xmac)
{
    uint32_t offset = ((xmac + 0x34U) & 0x3fffffffU) << 2;

    NPPT_U32(offset) = NPPT_U32(offset) | 0x410U;
}

void greg_smac_mask_runt_err(void)
{
    greg_smac0_3_mask_runt_err(0U);
    greg_smac0_3_mask_runt_err(1U);
    greg_smac0_3_mask_runt_err(2U);
    greg_smac0_3_mask_runt_err(3U);
    greg_smac6_mask_runt_err();
    greg_xmac_mask_runt_type(0U);
    greg_xmac_mask_runt_type(1U);
}

int greg_init(void)
{
    uint32_t usable_normal_buffer;

    NPPT_U32(0x68U) = uNORMAL_BP_SIZE | (uJUMBO_BP_SIZE << 16);
    usable_normal_buffer = uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE -
                           uBP_BUFFER_OFFSET - 63U;
    NPPT_U32(0x6cU) = usable_normal_buffer;
    NPPT_U32(0x20078U) = usable_normal_buffer;
    greg_smac_mask_runt_err();
    return 0;
}

void greg_rgmii_intf_mode_set(uint8_t mode)
{
    uint32_t value = NPPT_U32(0x30U) & 0xfff9ffffU;

    if (mode == 0U)
        value |= 0x60000U;
    NPPT_U32(0x30U) = value;
}

int greg_sdet_share_clk_cfg(uint32_t enable)
{
    uint32_t value;

    if (enable > 1U) {
        printk("PARA ERROR:greg_sdet_share_clk_cfg set failed! <para:%#x>\n",
               enable);
        return -1;
    }

    value = NPPT_U32(0x19cU);
    NPPT_U32(0x19cU) = (value & ~1U) | enable;
    return (int)enable;
}
