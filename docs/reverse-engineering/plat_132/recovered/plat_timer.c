/*
 * Semantic reconstruction of the module-local Timer0/Timer1 subsystem.
 *
 * Raw offsets, literal divisors, OF compatible strings, and ordering derive
 * from plat_132.ko. Timer register field names remain intentionally generic.
 */

#include <stdint.h>

typedef void (*timer0_callback_t)(void);
typedef int (*timer_irq_handler_t)(int irq, void *dev_id);

typedef struct zte_timer_tasklet {
    struct zte_timer_tasklet *next;
    volatile unsigned long state;
    int32_t count;
    uint32_t opaque_14;
    void (*function)(unsigned long data);
    unsigned long data;
} zte_timer_tasklet_t;

typedef struct zte_of_node zte_of_node_t;

#define TIMER_U32(base, offset) \
    (*(volatile uint32_t *)((volatile uint8_t *)(base) + (offset)))

extern int printk(const char *format, ...);
extern void dsb_st(void);
extern void enable_irq(unsigned int irq);
extern void disable_irq_nosync(unsigned int irq, unsigned int unused);
extern void __tasklet_hi_schedule(zte_timer_tasklet_t *tasklet);
extern int irq_set_affinity_hint(unsigned int irq, const void *mask);
extern int request_threaded_irq(unsigned int irq, timer_irq_handler_t handler,
                                void *thread_handler, unsigned long flags,
                                const char *name, void *dev_id);
extern zte_of_node_t *of_find_matching_node_and_match(
    zte_of_node_t *from, const void *matches, const void **matched);
extern int of_device_is_compatible(const zte_of_node_t *node,
                                   const char *compatible);
extern volatile uint8_t *of_iomap(const zte_of_node_t *node,
                                  unsigned int resource_index);
extern int of_irq_get(const zte_of_node_t *node, unsigned int index);

extern timer0_callback_t timer0_func;
extern int g_timer0_irq_2544;
extern uint32_t timer0_int_cnt;
extern volatile uint8_t *g_timer0_base_2544;
extern volatile uint8_t *g_timer1_base;
extern volatile uint8_t *g_lsp0_base;
extern zte_timer_tasklet_t timer0_tasklet;
extern unsigned long cpu_bit_bitmap[];
extern const uint8_t zx_timer_match[];

uint32_t __raw_readl(const volatile uint32_t *address)
{
    return *address;
}

void timer_refresh_config_load_reg(volatile uint8_t *base)
{
    uint32_t value = __raw_readl(
        (const volatile uint32_t *)(base + 0x10U));

    TIMER_U32(base, 0x10U) = value ^ 0x0fU;
}

static void timer0_schedule_high(void)
{
    volatile unsigned long *state = &timer0_tasklet.state;

    if ((__atomic_load_n(state, __ATOMIC_ACQUIRE) & 1UL) != 0)
        return;

    for (;;) {
        unsigned long old_state = __atomic_load_n(state, __ATOMIC_ACQUIRE);
        unsigned long new_state;

        if ((old_state & 1UL) != 0)
            return;
        new_state = old_state | 1UL;
        if (__atomic_compare_exchange_n(state, &old_state, new_state, 0,
                                        __ATOMIC_RELEASE,
                                        __ATOMIC_RELAXED)) {
            __atomic_thread_fence(__ATOMIC_SEQ_CST);
            __tasklet_hi_schedule(&timer0_tasklet);
            return;
        }
    }
}

void timer0_process(unsigned long unused)
{
    (void)unused;

    if (timer0_func != 0)
        timer0_func();
    enable_irq((unsigned int)g_timer0_irq_2544);
    ++timer0_int_cnt;
}

int timer_int_handler(int irq, void *dev_id)
{
    (void)irq;
    (void)dev_id;

    disable_irq_nosync((unsigned int)g_timer0_irq_2544, 0U);
    timer0_schedule_high();
    return 1;
}

void timer0_config(unsigned int hz)
{
    if (hz == 0U || g_timer0_base_2544 == 0)
        return;

    TIMER_U32(g_timer0_base_2544, 0x08U) = 25000000U / hz;
    TIMER_U32(g_timer0_base_2544, 0x04U) = 2U;
    timer_refresh_config_load_reg(g_timer0_base_2544);
}

void timer0_start(void)
{
    if (g_timer0_base_2544 != 0)
        TIMER_U32(g_timer0_base_2544, 0x0cU) = 1U;
}

void timer0_stop(void)
{
    if (g_timer0_base_2544 != 0)
        TIMER_U32(g_timer0_base_2544, 0x0cU) = 0U;
}

void zx_timer0_stop(void)
{
    timer0_stop();
}

void timer1_init(void)
{
    if (g_timer1_base == 0)
        return;

    TIMER_U32(g_timer1_base, 0x08U) = 0xffffffffU;
    TIMER_U32(g_timer1_base, 0x04U) = 2U;
    timer_refresh_config_load_reg(g_timer1_base);
    dsb_st();
    TIMER_U32(g_timer1_base, 0x0cU) = 1U;
}

uint32_t timer1_get_counter(void)
{
    if (g_timer1_base == 0)
        return 0;

    return __raw_readl((const volatile uint32_t *)(g_timer1_base + 0x18U));
}

timer0_callback_t timer0_register_func(timer0_callback_t callback)
{
    timer0_func = callback;
    return callback;
}

void zx_timer_wclk_sel(unsigned int select)
{
    uint32_t bit;
    uint32_t value;

    if (g_lsp0_base == 0)
        return;

    bit = (select & 1U) << 9;
    value = __raw_readl((const volatile uint32_t *)(g_lsp0_base + 0x04U));
    TIMER_U32(g_lsp0_base, 0x04U) = (value & ~0x200U) | bit;
    value = __raw_readl((const volatile uint32_t *)(g_lsp0_base + 0x08U));
    TIMER_U32(g_lsp0_base, 0x08U) = (value & ~0x200U) | bit;
}

void zx_timer_init(void)
{
    zte_of_node_t *node = 0;
    int irq;

    while ((node = of_find_matching_node_and_match(node, zx_timer_match, 0)) != 0) {
        if (of_device_is_compatible(node, "zxic,apb-timer0")) {
            g_timer0_base_2544 = of_iomap(node, 0U);
            if (g_timer0_base_2544 == 0) {
                printk("could not remap timer0 base\n");
                return;
            }

            irq = of_irq_get(node, 0U);
            g_timer0_irq_2544 = irq;
            if (irq <= 0) {
                printk("%s: failed to get timer0 irq\n",
                       "zx_timer_irq_base_of_init");
                return;
            }
            printk("get timer0 irq succeed,g_timer0_irq_2544:%d\n", irq);
        }

        if (of_device_is_compatible(node, "zxic,apb-timer1")) {
            g_timer1_base = of_iomap(node, 0U);
            if (g_timer1_base == 0) {
                printk("could not remap timer1 base\n");
                return;
            }
        }

        if (of_device_is_compatible(node, "zte,lsp0_crm")) {
            g_lsp0_base = of_iomap(node, 0U);
            if (g_lsp0_base == 0) {
                printk("could not remap lsp0 base\n");
                return;
            }
        }
    }

    if (g_timer0_base_2544 == 0 || g_timer1_base == 0 ||
        g_lsp0_base == 0)
        return;

    zx_timer_wclk_sel(1U);
    irq = request_threaded_irq((unsigned int)g_timer0_irq_2544,
                               timer_int_handler, 0, 0UL, "zx timer0", 0);
    if (irq < 0)
        printk("timer0 request_irq failed,irq %d\n", g_timer0_irq_2544);
}

void timer0_config_dothz(unsigned int rate)
{
    if (rate == 0U || g_timer0_base_2544 == 0)
        return;

    TIMER_U32(g_timer0_base_2544, 0x08U) = 250000000U / rate;
    TIMER_U32(g_timer0_base_2544, 0x04U) = 2U;
    timer_refresh_config_load_reg(g_timer0_base_2544);
}

static const void *timer_cpu_mask(unsigned int cpu)
{
    return (const uint8_t *)cpu_bit_bitmap + 8U * (cpu & 63U) + 8U -
           8U * (cpu >> 6);
}

void zx_timer0_start(unsigned int rate, unsigned int cpu,
                     timer0_callback_t callback)
{
    if (g_timer0_irq_2544 == 0)
        zx_timer_init();

    timer0_stop();
    if (callback != 0)
        timer0_func = callback;
    timer0_config_dothz(rate);
    irq_set_affinity_hint((unsigned int)g_timer0_irq_2544,
                          timer_cpu_mask(cpu));
    timer0_start();
}
