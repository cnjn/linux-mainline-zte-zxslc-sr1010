/*
 * Semantic reconstruction of CPU TX dispatch at 0x0d668.
 *
 * Descriptor-writing helpers below name inlined vendor blocks whose detailed
 * field layout is recorded separately. They are not claims about original
 * source-level helper names.
 */

#include <stdint.h>

typedef struct zte_net_device zte_net_device_t;
typedef struct zte_skb zte_skb_t;
typedef struct zte_tx_descriptor zte_tx_descriptor_t;

typedef struct {
    uint8_t opaque_0[16];
    uint64_t expires;
    uint8_t opaque_18[16];
} zte_timer_t;

typedef struct zte_tx_queue {
    uint8_t *descriptor_base;
    uintptr_t *owners;
    uint32_t producer;
    uint32_t pending;
    uint32_t consumer;
    uint32_t hardware_queue;
    uint32_t depth;
    uint32_t hardware_done;
} zte_tx_queue_t;

typedef struct {
    uint8_t opaque_0[8];
    uint64_t tx_packets;
    uint8_t opaque_10[8];
    uint64_t tx_bytes;
    uint8_t opaque_20[24];
    uint64_t tx_dropped;
} zte_tx_stats_t;

typedef struct {
    uint32_t producer;
    uint32_t consumer;
    uint32_t mask;
    uint32_t opaque_c;
    uint32_t lock;
    uint32_t opaque_14;
    void **entries;
} zte_buf_fifo_t;

typedef struct {
    void *objects[64];
    uint32_t count;
} zte_buf_fifo_staging_t;

typedef struct {
    uint32_t empty_count;
    uint32_t batch_refill_count;
    uint32_t irq_dequeue_count;
    uint32_t no_room_count;
    uint32_t irq_enqueue_count;
    uint32_t batch_enqueue_count;
    uint32_t high_context_call_count;
    uint32_t irq_context_call_count;
    uint32_t high_pop_count;
    uint32_t irq_dequeue_attempt_count;
    uint32_t null_result_count;
    uint8_t opaque_2c[0x14];
} zte_buf_fifo_counters_t;

typedef struct {
    uint8_t opaque_0[0x100];
    void *objects[64];
    uint32_t count;
} zte_buf_fifo_alloc_staging_t;

typedef int (*zte_tx_submit_t)(zte_skb_t *skb, zte_tx_descriptor_t *descriptor);

typedef struct {
    uintptr_t completion_ring_base;
    uint32_t ring_size;
    uint32_t release_index;
    uint32_t release_count;
} zte_recycle_context_t;

typedef void (*zte_recycle_callback_t)(unsigned int queue,
                                       zte_recycle_context_t *context);

struct cpu_net_ops_tx {
    uint8_t opaque_0[0x50];
    uint32_t (*get_tx_done)(unsigned int queue);
    uint32_t (*get_reorder_rls)(unsigned int queue);
    void (*update_reorder_rls)(uint32_t count_0, uint32_t count_1,
                               uint32_t count_2);
    zte_tx_submit_t cpu_tx;
    uint8_t opaque_70[8];
    zte_tx_submit_t omci_tx;
    zte_tx_submit_t wifi_tx;
};

struct cpu_tx_lock_state {
    unsigned long flags;
    int irqsave;
};

#define NETDEV_U32(device, offset) \
    (*(uint32_t *)((uint8_t *)(device) + (offset)))
#define SKB_U32(skb, offset) \
    (*(uint32_t *)((uint8_t *)(skb) + (offset)))
#define SKB_PTR(skb, offset) \
    (*(void **)((uint8_t *)(skb) + (offset)))
#define TXD_U32(descriptor, offset) \
    (*(uint32_t *)((uint8_t *)(descriptor) + (offset)))
#define TXD_U16(descriptor, offset) \
    (*(uint16_t *)((uint8_t *)(descriptor) + (offset)))
#define TXD_U8(descriptor, offset) \
    (*(uint8_t *)((uint8_t *)(descriptor) + (offset)))
#define SKB_U8(skb, offset) \
    (*(uint8_t *)((uint8_t *)(skb) + (offset)))

static const uint8_t idm_tx_test_packet_template[60] = {
    0x00U, 0xe0U, 0x42U, 0x68U, 0x14U, 0x20U, 0x74U, 0x4aU, 0xa4U, 0x0fU,
    0x4dU, 0xacU, 0x08U, 0x00U, 0x45U, 0x80U, 0x00U, 0x26U, 0x7fU, 0xbfU,
    0x40U, 0x00U, 0x32U, 0x11U, 0x50U, 0x28U, 0x0aU, 0x38U, 0x1cU, 0x46U,
    0x0aU, 0xe5U, 0x46U, 0xfdU, 0x13U, 0xecU, 0xadU, 0x8eU, 0x00U, 0x12U,
    0xcdU, 0x06U, 0x00U, 0x00U, 0x01U, 0x20U, 0x53U, 0x3aU, 0xa4U, 0x6eU,
    0x00U, 0x20U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
};

extern int all_kmodules_are_already;
extern uint32_t g_pon_work_mode;
extern uint32_t lan_up;
extern uint32_t lan_up_port;
extern uint32_t pon_up_flag;
extern uint32_t net_lock_tx;
extern uint32_t omcioam_lock_tx;
extern uint32_t idm_lock_tx;
extern uint32_t net_tx_drop;
extern uint32_t net_tx_done_total;
extern uint32_t net_tx_owner_error;
extern uint32_t net_nbuf_done_count;
extern uint32_t g_net_check_threshold;
extern uint32_t net_gso_cnt;
extern uint32_t net_gso_tcp_attempts;
extern uint32_t net_gso_tcp_failures;
extern uint32_t net_gso_non_tcp;
extern uint32_t gso_upload_mode;
extern unsigned long jiffies;
extern uint32_t idm_use_cpu_gso;
extern zte_net_device_t *cpu_netdev_slots[4];
extern zte_tx_queue_t *cpu_tq;
extern zte_tx_queue_t *omcioam_tq;
extern zte_tx_queue_t *idm_tq;
extern zte_tx_queue_t *unlock_tq[2];
extern int ipsec_tx_cpu;
extern uint32_t rls_ring_num_max;
extern zte_recycle_callback_t idm_recycle_cb[3];
extern uintptr_t tx_cmpl_ring_base[3];
extern uint32_t rls_ring_size[3];
extern uint32_t idm_rls_idx[3];
extern uint32_t idm_rls_cnt[3];
extern zte_buf_fifo_t buf_fifo[];
extern zte_buf_fifo_counters_t buf_fifo_cnt[];
extern uint8_t wifi0_free_data[];
extern uint8_t wifi1_free_data[];
extern uint8_t skb_free_data[];
extern uint8_t kmem_free_data[];
extern uint32_t idm_skb_pop_too_small_count[2];
extern uint32_t idmSkbRecycleLen;
extern uint32_t idm_skb_stack_reject_count[4];
extern const char *fifo_name[4];
extern const uint8_t __cpu_possible_mask[];
extern uint32_t nr_cpu_ids;
extern uint64_t __per_cpu_offset[];
extern struct cpu_net_ops_tx *cpu_net_ops;
extern int (*omci_mic_add)(const void *data, unsigned int length);
extern unsigned int (*dev_qos_select_queue)(zte_skb_t *skb);

extern uintptr_t read_sp_el0(void);
extern uintptr_t read_tpidr_el1(void);
extern unsigned long read_daif(void);
extern void daifset_irq(void);
extern void write_daif(unsigned long flags);
extern void queued_spin_lock_slowpath(uint32_t *lock, uint32_t observed,
                                       uintptr_t argument_2,
                                       unsigned int argument_3);
unsigned long __raw_spin_lock_irqsave(volatile uint32_t *lock);
void arch_local_irq_restore(unsigned long flags);
extern zte_tx_stats_t *cpu_dev_stat(zte_net_device_t *device);
extern void cpu_net_free_buf(void *buffer, unsigned int pool);
extern void __dev_kfree_skb_any(zte_skb_t *skb, unsigned int reason);
extern void kfree_skb_without_data(void *object);
extern void kmem_cache_free(void *cache, void *object);
extern void *kmem_buf_cache;
extern int printk(const char *format, ...);
extern void *memcpy(void *destination, const void *source, unsigned long size);
extern void *memset(void *destination, int value, unsigned long size);
extern void *__kmalloc(unsigned long size, unsigned int allocation_flags);
extern zte_skb_t *__alloc_skb(unsigned int size, unsigned int allocation_flags,
                              int argument_2, int argument_3);
extern void *skb_put(zte_skb_t *skb, unsigned int length);
extern zte_skb_t *skb_copy(zte_skb_t *skb, unsigned int allocation_flags);
extern unsigned int cpumask_next(int previous, const void *mask);
extern void skb_recycle(zte_skb_t *skb);
extern int pon_is_registered(void);
extern void ffe_learn_skb(zte_skb_t *skb, unsigned int source);
extern int net_gso_tx(zte_skb_t *skb, zte_net_device_t *device,
                      unsigned int path);
extern int net_tcp_gso_tx(zte_skb_t *skb, zte_net_device_t *device,
                          unsigned int path);
extern int net_tcp_gso_tx_upload(zte_skb_t *skb, zte_net_device_t *device,
                                 unsigned int path);
extern int net_tcp_gso_tx_upload1(zte_skb_t *skb, zte_net_device_t *device,
                                  unsigned int path);
extern zte_tx_descriptor_t *net_get_next_txdesc(zte_tx_queue_t *queue);
extern void net_set_prev_txdesc(zte_tx_queue_t *queue);
extern void net_tx_store_owner(zte_tx_queue_t *queue,
                               zte_tx_descriptor_t *descriptor,
                               zte_skb_t *skb);
extern void cpu_lowpower_tx(zte_net_device_t *device, zte_skb_t *skb,
                            zte_tx_descriptor_t *descriptor);
extern void cpu_net_sw_set_desc(zte_skb_t *skb, zte_tx_descriptor_t *descriptor);
extern void cpu_net_pon_set_desc(zte_skb_t *skb,
                                 zte_tx_descriptor_t *descriptor);
extern void cpu_net_omcioam_set_desc(zte_skb_t *skb,
                                     zte_tx_descriptor_t *descriptor);
extern void cpu_net_pad_to_minimum(zte_skb_t *skb, unsigned int length);
extern void cpu_net_free_nbuf(void *buffer);
int _idm_skb_stack_push(zte_skb_t *skb, unsigned int selector);
void idm_skb_stack_wifi_push(zte_skb_t *skb);
extern int add_timer_on(zte_timer_t *timer, int cpu);
extern zte_timer_t cpu_net_timer;
extern zte_timer_t cpu_unlock_timer[2];

void do_raw_spin_lock(volatile uint32_t *lock)
{
    __builtin_prefetch((const void *)lock, 1, 1);
    for (;;) {
        uint32_t observed = __atomic_load_n(lock, __ATOMIC_ACQUIRE);
        uint32_t expected = 0;

        if (observed != 0) {
            queued_spin_lock_slowpath((uint32_t *)lock, observed, 0, 1);
            return;
        }
        if (__atomic_compare_exchange_n(lock, &expected, 1U, 0,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            return;
    }
}

unsigned long __raw_spin_lock_irqsave(volatile uint32_t *lock)
{
    unsigned long flags = read_daif();

    if ((flags & 0x80U) == 0)
        daifset_irq();

    __builtin_prefetch((const void *)lock, 1, 1);
    for (;;) {
        uint32_t observed = __atomic_load_n(lock, __ATOMIC_ACQUIRE);
        uint32_t expected = 0;

        if (observed != 0) {
            queued_spin_lock_slowpath((uint32_t *)lock, observed, 0, 1);
            break;
        }
        if (__atomic_compare_exchange_n(lock, &expected, 1U, 0,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            break;
    }

    return flags;
}

unsigned long arch_local_irq_save(void)
{
    unsigned long flags = read_daif();

    if ((flags & 0x80U) == 0)
        daifset_irq();

    return flags;
}

void arch_local_irq_restore(unsigned long flags)
{
    write_daif(flags);
}

void arch_local_irq_restore_0(unsigned long flags)
{
    write_daif(flags);
}

uintptr_t __my_cpu_offset(void)
{
    return read_tpidr_el1();
}

void do_raw_spin_lock_flags_isra_2(volatile uint32_t *lock)
{
    __builtin_prefetch((const void *)lock, 1, 1);
    for (;;) {
        uint32_t observed = __atomic_load_n(lock, __ATOMIC_ACQUIRE);
        uint32_t expected = 0;

        if (observed != 0) {
            queued_spin_lock_slowpath((uint32_t *)lock, observed, 0, 1);
            return;
        }
        if (__atomic_compare_exchange_n(lock, &expected, 1U, 0,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            return;
    }
}

void do_raw_spin_lock_0(volatile uint32_t *lock)
{
    __builtin_prefetch((const void *)lock, 1, 1);
    for (;;) {
        uint32_t observed = __atomic_load_n(lock, __ATOMIC_ACQUIRE);
        uint32_t expected = 0;

        if (observed != 0) {
            queued_spin_lock_slowpath((uint32_t *)lock, observed, 0, 1);
            return;
        }
        if (__atomic_compare_exchange_n(lock, &expected, 1U, 0,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            return;
    }
}

void _buf_fifo_free_data(unsigned int selection, void *object)
{
    if (selection == 0) {
        kfree_skb_without_data(object);
    } else if (selection == 1) {
        kmem_cache_free(kmem_buf_cache, object);
    } else {
        __dev_kfree_skb_any((zte_skb_t *)object, 1U);
    }
}

uint32_t buf_fifo_free_data(zte_buf_fifo_staging_t *staging,
                            unsigned int selection, void *object)
{
    zte_buf_fifo_t *fifo = &buf_fifo[selection];
    zte_buf_fifo_counters_t *counters = &buf_fifo_cnt[selection];

    if ((*(const uint32_t *)(read_sp_el0() + 0x10) & 0xff00U) != 0) {
        uint32_t count = staging->count++;

        staging->objects[count] = object;
        if (staging->count > 31U) {
            uint32_t free_slots;

            do_raw_spin_lock_0(&fifo->lock);
            free_slots = fifo->mask + fifo->consumer - fifo->producer + 1U;
            if (free_slots <= 31U) {
                uint32_t i;

                __atomic_store_n((uint8_t *)&fifo->lock, 0, __ATOMIC_RELEASE);
                ++counters->no_room_count;
                for (i = 0; i < 32U; ++i)
                    _buf_fifo_free_data(selection, staging->objects[i]);
            } else {
                uint32_t start = fifo->producer & fifo->mask;
                uint32_t first = fifo->mask + 1U - start;

                if (first > 32U)
                    first = 32U;
                memcpy(&fifo->entries[start], staging->objects,
                       (unsigned long)first * sizeof(staging->objects[0]));
                if (first != 32U)
                    memcpy(fifo->entries, &staging->objects[first],
                           (unsigned long)(32U - first) *
                               sizeof(staging->objects[0]));
                fifo->producer += 32U;
                __atomic_store_n((uint8_t *)&fifo->lock, 0, __ATOMIC_RELEASE);
                ++counters->batch_enqueue_count;
            }
            staging->count = 0;
        }

        return ++counters->high_context_call_count;
    }

    {
        unsigned long flags = arch_local_irq_save();

        do_raw_spin_lock_flags_isra_2(&fifo->lock);
        if (fifo->mask + fifo->consumer - fifo->producer == 0xffffffffU) {
            __atomic_store_n((uint8_t *)&fifo->lock, 0, __ATOMIC_RELEASE);
            arch_local_irq_restore_0(flags);
            ++counters->no_room_count;
            _buf_fifo_free_data(selection, object);
        } else {
            fifo->entries[fifo->producer & fifo->mask] = object;
            ++fifo->producer;
            ++counters->irq_enqueue_count;
            __atomic_store_n((uint8_t *)&fifo->lock, 0, __ATOMIC_RELEASE);
            arch_local_irq_restore_0(flags);
        }
    }

    return ++counters->irq_context_call_count;
}

void *buf_fifo_alloc_data(zte_buf_fifo_alloc_staging_t *staging,
                          unsigned int selection)
{
    zte_buf_fifo_t *fifo = &buf_fifo[selection];
    zte_buf_fifo_counters_t *counters = &buf_fifo_cnt[selection];
    void *object;

    if ((*(const uint32_t *)(read_sp_el0() + 0x10) & 0xff00U) != 0) {
        if (staging->count == 0) {
            uint32_t available;

            do_raw_spin_lock_0(&fifo->lock);
            available = fifo->producer - fifo->consumer;
            if (available == 0) {
                ++counters->empty_count;
                __atomic_store_n((uint8_t *)&fifo->lock, 0, __ATOMIC_RELEASE);
            } else {
                uint32_t start = fifo->consumer & fifo->mask;
                uint32_t count = available > 32U ? 32U : available;
                uint32_t first = fifo->mask + 1U - start;

                if (first > count)
                    first = count;
                memcpy(staging->objects, &fifo->entries[start],
                       (unsigned long)first * sizeof(staging->objects[0]));
                if (first != count)
                    memcpy(&staging->objects[first], fifo->entries,
                           (unsigned long)(count - first) *
                               sizeof(staging->objects[0]));
                fifo->consumer += count;
                ++counters->batch_refill_count;
                __atomic_store_n((uint8_t *)&fifo->lock, 0, __ATOMIC_RELEASE);
                staging->count = count;
            }
        }

        if (staging->count != 0) {
            object = staging->objects[--staging->count];
            ++counters->high_pop_count;
            if (object != 0)
                return object;
        }

        ++counters->null_result_count;
        return 0;
    }

    {
        unsigned long flags = arch_local_irq_save();

        do_raw_spin_lock_flags_isra_2(&fifo->lock);
        if (fifo->producer == fifo->consumer) {
            ++counters->empty_count;
            object = 0;
        } else {
            uint32_t slot = fifo->consumer & fifo->mask;

            ++fifo->consumer;
            ++counters->irq_dequeue_count;
            object = fifo->entries[slot];
        }
        __atomic_store_n((uint8_t *)&fifo->lock, 0, __ATOMIC_RELEASE);
        arch_local_irq_restore_0(flags);
    }

    ++counters->irq_dequeue_attempt_count;
    if (object != 0)
        return object;

    ++counters->null_result_count;
    return 0;
}

zte_skb_t *idm_skb_stack_pop(uint8_t selector, int32_t minimum_size)
{
    unsigned int index = selector & 1U;
    uint8_t *staging_base = index != 0 ? wifi1_free_data : wifi0_free_data;
    zte_skb_t *skb = buf_fifo_alloc_data(
        (zte_buf_fifo_alloc_staging_t *)(staging_base + __my_cpu_offset()),
        index != 0 ? 3U : 2U);

    if (skb != 0) {
        uint32_t old_status = SKB_U32(skb, 0x114);

        skb_recycle(skb);
        if ((old_status & 1U) != 0)
            SKB_U32(skb, 0x114) |= 1U;
        if ((int64_t)((uint64_t)SKB_U32(skb, 0x120) - 64U) < minimum_size) {
            __dev_kfree_skb_any(skb, 1U);
            ++idm_skb_pop_too_small_count[index];
            return 0;
        }
    }

    return skb;
}

zte_skb_t *net_alloc_skb(void)
{
    return buf_fifo_alloc_data(
        (zte_buf_fifo_alloc_staging_t *)(skb_free_data + __my_cpu_offset()), 0);
}

void *net_alloc_kmem(void)
{
    return buf_fifo_alloc_data(
        (zte_buf_fifo_alloc_staging_t *)(kmem_free_data + __my_cpu_offset()), 1);
}

uint32_t net_free_kmem(void *object)
{
    return buf_fifo_free_data(
        (zte_buf_fifo_staging_t *)(kmem_free_data + __my_cpu_offset()), 1,
        object);
}

void buf_fifo_rls(unsigned int selection)
{
    uint8_t *staging_base;
    zte_buf_fifo_t *fifo;
    int32_t released = 0;
    int32_t release_limit;

    if (selection > 3U)
        return;

    switch (selection) {
    case 0:
        staging_base = skb_free_data;
        break;
    case 1:
        staging_base = kmem_free_data;
        break;
    case 2:
        staging_base = wifi0_free_data;
        break;
    default:
        staging_base = wifi1_free_data;
        break;
    }

    fifo = &buf_fifo[selection];
    release_limit = (int32_t)(fifo->mask + 1U);
    while (released < release_limit) {
        void *object = buf_fifo_alloc_data(
            (zte_buf_fifo_alloc_staging_t *)(staging_base + __my_cpu_offset()),
            selection);

        if (object == 0)
            break;
        ++released;
        _buf_fifo_free_data(selection, object);
    }
}

void buf_fifo_rls_all(void)
{
    buf_fifo_rls(0U);
    buf_fifo_rls(1U);
    buf_fifo_rls(2U);
    buf_fifo_rls(3U);
}

void idm_recycle_stats(void)
{
    unsigned int selection;

    for (selection = 0; selection < 4U; ++selection) {
        zte_buf_fifo_t *fifo = &buf_fifo[selection];
        zte_buf_fifo_counters_t *counters = &buf_fifo_cnt[selection];

        printk("######buf fifo %s:\n", fifo_name[selection]);
        printk("  fifo in use %u\n", fifo->producer - fifo->consumer);
        printk("  fifo empty %u\n", counters->empty_count);
        printk("  fifo out batch %u\n", counters->batch_refill_count);
        printk("  fifo out single %u\n", counters->irq_dequeue_count);
        printk("  fifo alloc without lock %u\n", counters->high_pop_count);
        printk("  fifo alloc with lock %u\n", counters->irq_dequeue_attempt_count);
        printk("  fifo alloc fail %u\n", counters->null_result_count);
        printk("  fifo full %u\n", counters->no_room_count);
        printk("  fifo in batch %u\n", counters->batch_enqueue_count);
        printk("  fifo in single %u\n", counters->irq_enqueue_count);
        printk("  fifo free without lock %u\n",
               counters->high_context_call_count);
        printk("  fifo free with lock %u\n", counters->irq_context_call_count);
    }
}

void idm_recycle_init(void)
{
    unsigned int cpu = (unsigned int)-1;
    unsigned int selection;

    while ((cpu = cpumask_next((int)cpu, __cpu_possible_mask)) < nr_cpu_ids) {
        uintptr_t offset = (uintptr_t)__per_cpu_offset[cpu];
        uint8_t *staging;

        staging = skb_free_data + offset;
        *(uint32_t *)(void *)(staging + 0x200U) = 0;
        *(uint32_t *)(void *)(staging + 0x204U) = 0;
        staging = kmem_free_data + offset;
        *(uint32_t *)(void *)(staging + 0x200U) = 0;
        *(uint32_t *)(void *)(staging + 0x204U) = 0;
        staging = wifi0_free_data + offset;
        *(uint32_t *)(void *)(staging + 0x200U) = 0;
        *(uint32_t *)(void *)(staging + 0x204U) = 0;
        staging = wifi1_free_data + offset;
        *(uint32_t *)(void *)(staging + 0x200U) = 0;
        *(uint32_t *)(void *)(staging + 0x204U) = 0;
    }

    memset(buf_fifo, 0, 0x80U);
    for (selection = 0; selection < 4U; ++selection) {
        zte_buf_fifo_t *fifo = &buf_fifo[selection];

        fifo->mask = 0xfffU;
        fifo->lock = 0;
        fifo->entries = __kmalloc((uint64_t)(fifo->mask + 1U) << 3,
                                  0xa20U);
    }
}

void idm_skb_stack_wifi_push(zte_skb_t *skb)
{
    uint32_t status = SKB_U32(skb, 0x114);

    if ((status & 0x20000U) != 0) {
        _idm_skb_stack_push(skb, 0);
    } else if ((status & 0x40000U) != 0) {
        _idm_skb_stack_push(skb, 1);
    } else {
        __dev_kfree_skb_any(skb, 1U);
    }
}

int _idm_skb_stack_push(zte_skb_t *skb, unsigned int selector)
{
    uint32_t recycle_length;
    uint8_t selector_byte;
    uint8_t *staging_base;
    unsigned int fifo_selection;

    if (SKB_U32(skb, 0xac) != 0 || (SKB_U8(skb, 0xb6) & 0x0cU) != 0) {
        ++idm_skb_stack_reject_count[0];
        goto reject;
    }

    recycle_length = (idmSkbRecycleLen + 127U) & ~63U;
    if ((int64_t)(uint32_t)SKB_U32(skb, 0x120) <
        (int64_t)(int32_t)recycle_length) {
        ++idm_skb_stack_reject_count[1];
        goto reject;
    }

    if (SKB_U32(skb, 0x13c) != 1U) {
        ++idm_skb_stack_reject_count[2];
        goto reject;
    }

    if ((SKB_U8(skb, 0xb6) & 1U) != 0 &&
        (*(const uint32_t *)((const uint8_t *)SKB_PTR(skb, 0x128) +
                             SKB_U32(skb, 0x120) + 32U) & 0xffffU) != 1U) {
        ++idm_skb_stack_reject_count[3];
        goto reject;
    }

    selector_byte = (uint8_t)selector;
    staging_base = selector_byte != 0 ? wifi1_free_data : wifi0_free_data;
    fifo_selection = selector_byte != 0 ? 3U : 2U;
    buf_fifo_free_data(
        (zte_buf_fifo_staging_t *)(staging_base + __my_cpu_offset()),
        fifo_selection, skb);
    return 0;

reject:
    __dev_kfree_skb_any(skb, 1U);
    return 0;
}

void idm_skb_stack_push(zte_skb_t *skb)
{
    uint32_t status = SKB_U32(skb, 0x114);

    if ((status & 0x10001U) != 0x10001U) {
        __dev_kfree_skb_any(skb, 1U);
        return;
    }

    cpu_net_free_buf(SKB_PTR(skb, 0x128), 0U);
    buf_fifo_free_data(
        (zte_buf_fifo_staging_t *)(skb_free_data + __my_cpu_offset()), 0U,
        skb);
}

void dev_kfree_skb_any(zte_skb_t *skb)
{
    __dev_kfree_skb_any(skb, 1U);
}

static struct cpu_tx_lock_state cpu_tx_lock(volatile uint32_t *lock)
{
    struct cpu_tx_lock_state state = { 0, 0 };

    if ((read_sp_el0() & 0x1fff00U) != 0) {
        do_raw_spin_lock(lock);
    } else {
        state.flags = __raw_spin_lock_irqsave(lock);
        state.irqsave = 1;
    }
    return state;
}

static void cpu_tx_unlock(volatile uint32_t *lock,
                          struct cpu_tx_lock_state state)
{
    __atomic_store_n((uint8_t *)lock, 0, __ATOMIC_RELEASE);
    if (state.irqsave)
        arch_local_irq_restore(state.flags);
}

static int cpu_skb_needs_gso(const zte_skb_t *skb)
{
    const uint8_t *head = SKB_PTR(skb, 0x128);

    return *(const uint16_t *)(head + SKB_U32(skb, 0x120) + 4) != 0 ||
           SKB_U32(skb, 0xac) != 0;
}

static int cpu_skb_has_gso_size(const zte_skb_t *skb)
{
    const uint8_t *head = SKB_PTR(skb, 0x128);

    return *(const uint16_t *)(head + SKB_U32(skb, 0x120) + 4) != 0;
}

static int idm_skb_needs_cpu_gso(const zte_skb_t *skb)
{
    return (SKB_U32(skb, 0x114) & 0x4000U) != 0 || cpu_skb_needs_gso(skb);
}

static void cpu_tx_drop(zte_skb_t *skb, zte_net_device_t *device)
{
    zte_tx_stats_t *stats = cpu_dev_stat(device);

    if (stats != 0)
        ++stats->tx_dropped;
    dev_kfree_skb_any(skb);
}

static void cpu_tx_submit(zte_tx_queue_t *queue, zte_skb_t *skb,
                          zte_net_device_t *device,
                          zte_tx_descriptor_t *descriptor,
                          zte_tx_submit_t submit)
{
    if (submit(skb, descriptor) < 0) {
        net_set_prev_txdesc(queue);
        cpu_tx_drop(skb, device);
        return;
    }

    net_tx_store_owner(queue, descriptor, skb);
    {
        zte_tx_stats_t *stats = cpu_dev_stat(device);

        if (stats != 0) {
            ++stats->tx_packets;
            stats->tx_bytes += SKB_U32(skb, 0xa8);
        }
    }
}

int cpu_net_tx(zte_skb_t *skb, zte_net_device_t *device)
{
    unsigned int type;
    zte_tx_descriptor_t *descriptor;
    struct cpu_tx_lock_state lock_state;

    if (!all_kmodules_are_already) {
        dev_kfree_skb_any(skb);
        ++net_tx_drop;
        return 0;
    }

    type = NETDEV_U32(device, 0x888);
    switch (type) {
    case 1: /* sw */
        lock_state = cpu_tx_lock(&net_lock_tx);
        if (cpu_skb_needs_gso(skb)) {
            net_gso_tx(skb, device, 1);
            dev_kfree_skb_any(skb);
        } else if ((descriptor = net_get_next_txdesc(cpu_tq)) == 0) {
            cpu_tx_drop(skb, device);
        } else {
            cpu_net_sw_set_desc(skb, descriptor);
            cpu_lowpower_tx(device, skb, descriptor);
            cpu_tx_submit(cpu_tq, skb, device, descriptor, cpu_net_ops->cpu_tx);
        }
        cpu_tx_unlock(&net_lock_tx, lock_state);
        break;

    case 0: /* pon */
        lock_state = cpu_tx_lock(&net_lock_tx);
        if (!lan_up && ((g_pon_work_mode & 0xe40U) != 0 ||
                        (g_pon_work_mode & 0x1a0U) == 0 ||
                        !pon_is_registered())) {
            cpu_tx_drop(skb, device);
        } else {
            ffe_learn_skb(skb, 3);
            if (cpu_skb_needs_gso(skb)) {
                net_gso_tx(skb, device, 0);
                dev_kfree_skb_any(skb);
            } else if ((descriptor = net_get_next_txdesc(cpu_tq)) == 0) {
                cpu_tx_drop(skb, device);
            } else {
                cpu_net_pon_set_desc(skb, descriptor);
                cpu_net_pad_to_minimum(skb, 60);
                cpu_lowpower_tx(device, skb, descriptor);
                cpu_tx_submit(cpu_tq, skb, device, descriptor,
                              cpu_net_ops->cpu_tx);
            }
        }
        cpu_tx_unlock(&net_lock_tx, lock_state);
        break;

    case 2: /* omci/oam */
        lock_state = cpu_tx_lock(&omcioam_lock_tx);
        if ((g_pon_work_mode & 0x600U) != 0 && omci_mic_add != 0 &&
            omci_mic_add(SKB_PTR(skb, 0x130), SKB_U32(skb, 0xa8)) != 0) {
            cpu_tx_drop(skb, device);
        } else if ((descriptor = net_get_next_txdesc(omcioam_tq)) == 0) {
            cpu_tx_drop(skb, device);
        } else {
            cpu_net_omcioam_set_desc(skb, descriptor);
            cpu_tx_submit(omcioam_tq, skb, device, descriptor,
                          cpu_net_ops->omci_tx);
        }
        cpu_tx_unlock(&omcioam_lock_tx, lock_state);
        break;

    default:
        dev_kfree_skb_any(skb);
        ++net_tx_drop;
        break;
    }

    return 0;
}

int idm_tx_test(unsigned int port, unsigned int queue, void *unused_2,
                void *unused_3, unsigned int length,
                unsigned int packet_count)
{
    uint8_t template[sizeof(idm_tx_test_packet_template)];
    zte_skb_t *skb;
    zte_net_device_t *device;
    unsigned int template_length;
    unsigned int index;

    (void)unused_2;
    (void)unused_3;
    memcpy(template, idm_tx_test_packet_template, sizeof(template));
    if (packet_count == 0U)
        return 0;

    skb = __alloc_skb(length, 0xa20U, 0, -1);
    if (skb == 0) {
        printk("alloc skb failed\n");
        return -1;
    }

    template_length = length > sizeof(template) ? sizeof(template) : length;
    memcpy(SKB_PTR(skb, 0x130), template, template_length);
    for (index = 0; index < length - template_length; ++index)
        ((uint8_t *)SKB_PTR(skb, 0x130))[template_length + index] =
            (uint8_t)index;

    skb_put(skb, length);
    SKB_U32(skb, 0xa8) = length;
    if (port == 7U) {
        device = cpu_netdev_slots[0];
        SKB_PTR(skb, 0x10) = device;
    } else if (port == 0xffffU) {
        device = cpu_netdev_slots[2];
        SKB_PTR(skb, 0x10) = device;
        SKB_U8(skb, 0x108) = 0xffU;
    } else {
        SKB_U8(skb, 0x108) = (uint8_t)port;
        device = cpu_netdev_slots[1];
        SKB_PTR(skb, 0x10) = device;
    }

    printk("start send skb num %d,len %d, port %d, queue %d\n",
           (int)packet_count, (int)template_length, (int)port, (int)queue);
    while (packet_count != 1U) {
        zte_skb_t *copy = skb_copy(skb, 0xa20U);

        if (copy == 0) {
            printk("skb_copy failed\n");
            cpu_net_tx(skb, (zte_net_device_t *)SKB_PTR(skb, 0x10));
            return -1;
        }

        --packet_count;
        cpu_net_tx(copy, (zte_net_device_t *)SKB_PTR(copy, 0x10));
    }

    cpu_net_tx(skb, (zte_net_device_t *)SKB_PTR(skb, 0x10));
    return 0;
}

int idm_net_tx(zte_skb_t *skb, zte_net_device_t *device)
{
    zte_tx_descriptor_t *descriptor;
    struct cpu_tx_lock_state lock_state;

    if (idm_use_cpu_gso != 0 && idm_skb_needs_cpu_gso(skb)) {
        lock_state = cpu_tx_lock(&net_lock_tx);
        net_gso_tx(skb, device, 1);
        dev_kfree_skb_any(skb);
        cpu_tx_unlock(&net_lock_tx, lock_state);
        return 0;
    }

    lock_state = cpu_tx_lock(&idm_lock_tx);
    descriptor = net_get_next_txdesc(idm_tq);
    if (descriptor == 0) {
        cpu_tx_drop(skb, device);
        cpu_tx_unlock(&idm_lock_tx, lock_state);
        return -1;
    }

    TXD_U32(descriptor, 0x14) = 0x04000000U;
    if (cpu_net_ops->wifi_tx(skb, descriptor) != 0) {
        net_set_prev_txdesc(idm_tq);
        cpu_tx_drop(skb, device);
    } else {
        zte_tx_stats_t *stats;

        net_tx_store_owner(idm_tq, descriptor, skb);
        stats = cpu_dev_stat(device);
        if (stats != 0) {
            ++stats->tx_packets;
            stats->tx_bytes += SKB_U32(skb, 0xa8);
        }
    }

    cpu_tx_unlock(&idm_lock_tx, lock_state);
    return 0;
}

int net_gso_tx(zte_skb_t *skb, zte_net_device_t *device, unsigned int path)
{
    zte_tx_stats_t *stats;

    ++net_gso_cnt;
    SKB_PTR(skb, 0x10) = device;

    if ((SKB_U8(skb, 0xbb) & 0x10U) != 0) {
        int status = gso_upload_mode != 0 ?
            net_tcp_gso_tx_upload(skb, device, path) :
            net_tcp_gso_tx_upload1(skb, device, path);

        if (status >= 0)
            return 0;
        goto drop;
    }

    if ((SKB_U32(skb, 0x114) & 0x4000U) != 0 ||
        cpu_skb_has_gso_size(skb)) {
        ++net_gso_tcp_attempts;
        if (net_tcp_gso_tx(skb, device, path) >= 0) {
            stats = cpu_dev_stat(device);
            if (stats != 0) {
                ++stats->tx_packets;
                stats->tx_bytes += SKB_U32(skb, 0xa8);
            }
            return 0;
        }
        ++net_gso_tcp_failures;
    } else {
        ++net_gso_non_tcp;
    }

drop:
    stats = cpu_dev_stat(device);
    if (stats != 0)
        ++stats->tx_dropped;
    return 0;
}

uint32_t net_check_tx_done_nolock(zte_tx_queue_t *queue)
{
    uint32_t hardware_done = cpu_net_ops->get_tx_done(queue->hardware_queue);
    uint32_t old_hardware_done = queue->hardware_done;
    uint32_t completed;
    uint32_t pending_before;
    uint32_t i;

    if (hardware_done == old_hardware_done)
        return hardware_done;

    completed = hardware_done - old_hardware_done;
    if (hardware_done <= old_hardware_done)
        completed += 0x10000U;

    for (i = 0; i < completed; ++i) {
        uintptr_t owner = queue->owners[queue->consumer];

        if (owner == 0) {
            ++net_tx_owner_error;
        } else if ((owner & 3U) != 0) {
            cpu_net_free_nbuf((void *)(owner & ~(uintptr_t)3U));
            ++net_nbuf_done_count;
        } else if (queue == idm_tq) {
            idm_skb_stack_wifi_push((zte_skb_t *)owner);
        } else {
            dev_kfree_skb_any((zte_skb_t *)owner);
        }

        queue->owners[queue->consumer] = 0;
        ++queue->consumer;
        if (queue->consumer >= queue->depth)
            queue->consumer = 0;
    }

    net_tx_done_total += completed;
    pending_before = queue->pending;
    queue->hardware_done = hardware_done;
    queue->pending = pending_before - completed;
    return pending_before;
}

void net_check_reorder_rls_nolock(void)
{
    uint32_t released[3] = { 0, 0, 0 };
    uint32_t queue;

    for (queue = 0; queue < rls_ring_num_max; ++queue) {
        uint32_t count = cpu_net_ops->get_reorder_rls(queue);
        zte_recycle_callback_t callback;

        if (count > 0xfffU)
            count = 0xfffU;
        released[queue] = count;
        if (count == 0)
            continue;

        callback = idm_recycle_cb[queue];
        if (callback == 0)
            continue;

        {
            zte_recycle_context_t context = {
                tx_cmpl_ring_base[queue],
                rls_ring_size[queue],
                idm_rls_idx[queue],
                count,
            };

            callback(queue, &context);
        }

        idm_rls_idx[queue] =
            (count + idm_rls_idx[queue]) & (rls_ring_size[queue] - 1U);
        idm_rls_cnt[queue] += count;
    }

    cpu_net_ops->update_reorder_rls(released[0], released[1], released[2]);
}

static void cpu_timer_reclaim_queue(volatile uint32_t *lock,
                                     zte_tx_queue_t *queue)
{
    do_raw_spin_lock(lock);
    net_check_tx_done_nolock(queue);
    __atomic_store_n((uint8_t *)lock, 0, __ATOMIC_RELEASE);
}

void cpu_timer_unlock(zte_timer_t *timer)
{
    unsigned int timer_index = (unsigned int)(timer - cpu_unlock_timer);
    zte_timer_t *canonical_timer = &cpu_unlock_timer[timer_index];

    net_check_tx_done_nolock(unlock_tq[timer_index]);
    canonical_timer->expires = jiffies + 1;
    add_timer_on(canonical_timer, ipsec_tx_cpu);
}

void cpu_timer_func(zte_timer_t *timer)
{
    (void)timer;

    if ((uint16_t)g_net_check_threshold > 1U) {
        cpu_timer_reclaim_queue(&omcioam_lock_tx, omcioam_tq);
        cpu_timer_reclaim_queue(&net_lock_tx, cpu_tq);
    }

    do_raw_spin_lock(&idm_lock_tx);
    if ((uint16_t)g_net_check_threshold > 1U)
        net_check_tx_done_nolock(idm_tq);
    net_check_reorder_rls_nolock();
    __atomic_store_n((uint8_t *)&idm_lock_tx, 0, __ATOMIC_RELEASE);

    cpu_net_timer.expires = jiffies + 1;
    add_timer_on(&cpu_net_timer, 0);
}

void cpu_net_pon_set_desc(zte_skb_t *skb, zte_tx_descriptor_t *descriptor)
{
    TXD_U32(descriptor, 0x18) = 0x08000000U;
    TXD_U32(descriptor, 0x10) = 0;
    TXD_U32(descriptor, 0x14) = 0;

    if ((g_pon_work_mode & 0x10U) == 0) {
        SKB_U8(skb, 0x108) = 0;
        return;
    }

    if (dev_qos_select_queue != 0) {
        TXD_U16(descriptor, 0x1a) =
            (TXD_U16(descriptor, 0x1a) & 0xfe00U) |
            (dev_qos_select_queue(skb) & 0x1ffU);
    } else {
        TXD_U8(descriptor, 0x1a) = 0;
        TXD_U8(descriptor, 0x1b) &= 0xfeU;
    }

    if (pon_up_flag != 1) {
        ++SKB_U8(skb, 0x108);
    } else if (lan_up == 1) {
        SKB_U8(skb, 0x108) = (uint8_t)lan_up_port;
    } else {
        SKB_U8(skb, 0x108) = 0;
    }
}
