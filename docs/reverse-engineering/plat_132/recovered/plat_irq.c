/*
 * Semantic reconstruction of plat_132 top-level interrupt registration and
 * dispatch helpers.
 *
 * The request_threaded_irq argument names follow generic kernel ABI
 * vocabulary only. Values and control flow are from the vendor binary.
 */

typedef int (*zte_irq_handler_t)(int irq, void *dev_id);
typedef void (*pon_protocol_isr_t)(unsigned int source, void *context);

extern int printk(const char *format, ...);
extern int request_threaded_irq(unsigned int irq,
                                zte_irq_handler_t handler,
                                zte_irq_handler_t thread_handler,
                                unsigned long flags,
                                const char *name,
                                void *dev_id);

extern int g_pon_irq;
extern int g_nppt_irq;
extern void *pon_int_info;
int zx_pon_int(int irq, void *dev_id);
int zx_nppt_int(int irq, void *dev_id);

extern volatile unsigned char *pon_base;
extern volatile unsigned char *nppt_base;
extern unsigned int g_pon_work_mode;
extern unsigned int ZX_INT_PON;
extern unsigned int pon_registered;
extern unsigned int rog_onu_flag;
extern unsigned int dg_flag;
extern unsigned int soam_alarm_flag;

extern pon_protocol_isr_t oam_isr;
extern pon_protocol_isr_t gpon_isr;
extern pon_protocol_isr_t xgpon_isr;
extern pon_protocol_isr_t epon_isr;
extern pon_protocol_isr_t xeupon_isr;
extern pon_protocol_isr_t xedpon_isr;
extern pon_protocol_isr_t lp_isr;
extern pon_protocol_isr_t low_power_isr;
extern pon_protocol_isr_t ptp_isr;
extern pon_protocol_isr_t ptp_stamp_isr;

extern unsigned int gpon_get_onu_state(void);
extern unsigned int xgpon_get_onu_state(void);
extern unsigned int epon_get_llid_state(void);
extern unsigned int xepon_get_llid_state(void);
extern void epon_set_dg_cnt(void);
extern void __const_udelay(unsigned long loops);
extern void hw_power_optx_set(unsigned int enabled);
extern void dg_timer_init(void);

#define PON_MMIO32(offset) (*(volatile unsigned int *)(pon_base + (offset)))
#define NPPT_MMIO32(offset) (*(volatile unsigned int *)(nppt_base + (offset)))

/*
 * The handler is registered with &pon_int_info. The binary derives this
 * adjacent counter from dev_id + 0x18 rather than from a separately named
 * data item.
 */
#define PON_IRQ_CALLBACK_COUNT(dev_id) \
    (*(unsigned int *)((unsigned char *)(dev_id) + 0x18))

/* These labels identify observed dispatch sites, not recovered vendor names. */
#define PON_IRQ_GPON       (1u << 0)
#define PON_IRQ_ROG_ONU    (1u << 4)
#define PON_IRQ_DG         (1u << 5)
#define PON_IRQ_LP         (1u << 6)
#define PON_IRQ_XGPON      (1u << 7)
#define PON_IRQ_EPON       (1u << 8)
#define PON_IRQ_XEPON      (1u << 9)
#define PON_IRQ_XEDPON     (1u << 10)
#define PON_IRQ_LOW_POWER  (1u << 11)

/* These labels identify observed dispatch sites, not recovered vendor names. */
#define NPPT_IRQ_OAM        (1u << 8)
#define NPPT_IRQ_PTP_STAMP  (1u << 9)
#define NPPT_IRQ_PTP        (1u << 10)

int register_pon_int(void)
{
    int ret;

    printk("pon int\n");
    ret = request_threaded_irq((unsigned int)g_pon_irq, zx_pon_int, 0, 0,
                               "pon", &pon_int_info);
    if (ret < 0) {
        printk("request pon irq failed\n");
        return ret;
    }

    return 0;
}

int register_nppt_int(void)
{
    int ret;

    printk("nppt int\n");
    ret = request_threaded_irq((unsigned int)g_nppt_irq, zx_nppt_int, 0, 0,
                               "nppt", &pon_int_info);
    if (ret < 0) {
        printk("request nppt irq failed\n");
        return ret;
    }

    return 0;
}

int zx_pon_int(int irq, void *dev_id)
{
    unsigned int active_status;
    unsigned int work_mode;

    (void)irq;
    active_status = PON_MMIO32(0x40) & ~PON_MMIO32(0x44);

    if ((active_status & PON_IRQ_GPON) != 0 && gpon_isr != 0) {
        gpon_isr(ZX_INT_PON, *(void **)dev_id);
        ++PON_IRQ_CALLBACK_COUNT(dev_id);
        pon_registered = gpon_get_onu_state() == 5;
    }

    if ((active_status & PON_IRQ_XGPON) != 0 && xgpon_isr != 0) {
        xgpon_isr(ZX_INT_PON, *(void **)dev_id);
        ++PON_IRQ_CALLBACK_COUNT(dev_id);
        pon_registered = xgpon_get_onu_state() == 5;
    }

    if ((active_status & PON_IRQ_EPON) != 0 && epon_isr != 0) {
        epon_isr(ZX_INT_PON, *(void **)dev_id);
        ++PON_IRQ_CALLBACK_COUNT(dev_id);
        if ((g_pon_work_mode & 0x20u) != 0)
            pon_registered = epon_get_llid_state() != 0;
    }

    if ((active_status & PON_IRQ_XEPON) != 0 && xeupon_isr != 0) {
        xeupon_isr(ZX_INT_PON, *(void **)dev_id);
        ++PON_IRQ_CALLBACK_COUNT(dev_id);
        if ((g_pon_work_mode & 0x180u) != 0)
            pon_registered = xepon_get_llid_state() != 0;
    }

    if ((active_status & PON_IRQ_XEDPON) != 0 && xedpon_isr != 0) {
        xedpon_isr(ZX_INT_PON, *(void **)dev_id);
        ++PON_IRQ_CALLBACK_COUNT(dev_id);
    }

    if ((active_status & PON_IRQ_LP) != 0 && lp_isr != 0) {
        lp_isr(ZX_INT_PON, *(void **)dev_id);
        ++PON_IRQ_CALLBACK_COUNT(dev_id);
    }

    if ((active_status & PON_IRQ_ROG_ONU) != 0)
        rog_onu_flag = 1;

    if ((active_status & PON_IRQ_LOW_POWER) != 0) {
        low_power_isr(ZX_INT_PON, *(void **)dev_id);
        ++PON_IRQ_CALLBACK_COUNT(dev_id);
    }

    if ((active_status & PON_IRQ_DG) != 0) {
        work_mode = g_pon_work_mode;
        if ((work_mode & 0x1a0u) != 0 && !dg_flag) {
            epon_set_dg_cnt();
            __const_udelay(0x8312b0);
            __const_udelay(0x8312b0);
            __const_udelay(0x8312b0);
            __const_udelay(0x418958);

            work_mode = g_pon_work_mode;
            if ((work_mode & 0xa0u) != 0)
                PON_MMIO32(0x180000) &= ~1u;
            if ((work_mode & 0x100u) != 0)
                PON_MMIO32(0x1c0004) &= ~1u;
            hw_power_optx_set(0);
            dg_timer_init();
            dg_flag = 1;
        }

        work_mode = g_pon_work_mode;
        if ((work_mode & 0x640u) != 0 && !dg_flag) {
            __const_udelay(0x8312b0);
            __const_udelay(0x8312b0);
            __const_udelay(0x8312b0);
            __const_udelay(0x418958);

            work_mode = g_pon_work_mode;
            if ((work_mode & 0x40u) != 0)
                PON_MMIO32(0x84000) &= 0xfffffff6u;
            if ((work_mode & 0x600u) != 0)
                PON_MMIO32(0x58400) &= ~1u;
            hw_power_optx_set(0);
            dg_timer_init();
            dg_flag = 1;
        }

        printk("DGi\n");
    }

    return 1;
}

int zx_nppt_int(int irq, void *dev_id)
{
    unsigned int active_status;

    (void)irq;
    active_status = NPPT_MMIO32(0) & ~NPPT_MMIO32(4);

    if ((active_status & NPPT_IRQ_OAM) != 0 && oam_isr != 0) {
        oam_isr(0, 0);
        soam_alarm_flag = 1;

        /* The binary performs these reads after a handled OAM event. */
        (void)NPPT_MMIO32(0x1c000);
        (void)NPPT_MMIO32(0x1c004);
        (void)NPPT_MMIO32(0x1c008);
        (void)NPPT_MMIO32(0x1c00c);
        (void)NPPT_MMIO32(0x1c2c4);
        (void)NPPT_MMIO32(0x1c2c8);
        (void)NPPT_MMIO32(0x1c2cc);
        (void)NPPT_MMIO32(0x1c2d0);
        (void)NPPT_MMIO32(0x1c2d4);
        (void)NPPT_MMIO32(0x1c2d8);
        (void)NPPT_MMIO32(0x1c2dc);
    }

    if ((active_status & NPPT_IRQ_PTP) != 0 && ptp_isr != 0)
        ptp_isr(0, 0);

    if ((active_status & NPPT_IRQ_PTP_STAMP) != 0 && ptp_stamp_isr != 0)
        ptp_stamp_isr(ZX_INT_PON, *(void **)dev_id);

    return 1;
}
