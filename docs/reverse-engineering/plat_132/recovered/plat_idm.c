/*
 * Semantic reconstruction of idm_init at 0x14ff4.
 *
 * Register offsets and control flow come from the vendor binary. Structure
 * field names are analyst labels where the module does not provide a name.
 */

#include <stddef.h>
#include <stdint.h>

#define IDM_RX_QUEUE_COUNT 24U
#define IDM_TX_QUEUE_COUNT 4U
#define IDM_REG_BASE 0x280000U
#define IDM_REFILL_BATCH 2048U
#define IDM_KMALLOC_FLAGS 2592U
#define IDM_CACHE_FLAGS 270336U

#define IDM_LINEAR_ADDRESS(physical) \
    ((uintptr_t)(((uint32_t)(physical) - memstart_addr) | 0xffffff8000000000ULL))
#define IDM_REG32(offset) \
    (*(volatile uint32_t *)(nppt_base + IDM_REG_BASE + (offset)))
#define IDM_AFFINITY_HINT(cpu) ((const void *)(cpu_bit_bitmap + (cpu) + 1U))

struct idm_rx_queue {
    uint8_t *descriptor_base;
    uint32_t producer;
    uint32_t depth;
};

struct idm_tx_queue {
    uint8_t *descriptor_base;
    void **buffer_table;
    uint32_t word_10;
    uint32_t word_14;
    uint32_t word_18;
    uint32_t queue_index;
    uint32_t depth;
    uint32_t word_24;
};

struct idm_fifo {
    uint32_t in;
    uint32_t out;
    uint32_t mask;
    uint32_t buffer_data_length;
    uint32_t lock_word;
    uint32_t pad;
    void **buffer;
};

struct idm_net_info {
    uint32_t word_0;
    uint32_t word_4;
    uint32_t word_8;
    uint32_t word_c;
    void *ops;
};

struct idm_refill_staging {
    uint32_t entries[2][32];
    uint32_t count[2];
};

struct idm_free_stash {
    void *producer_entries[32];
    void *consumer_entries[32];
    uint32_t producer_count;
    uint32_t consumer_count;
};

typedef int (*idm_irq_handler_t)(int irq, void *dev_id);
typedef struct zte_skb zte_skb_t;
typedef void (*idm_free_skb_data_t)(zte_skb_t *skb);

#define IDM_TXD_U32(descriptor, offset) \
    (*(uint32_t *)((uint8_t *)(descriptor) + (offset)))
#define IDM_TXD_U16(descriptor, offset) \
    (*(uint16_t *)((uint8_t *)(descriptor) + (offset)))
#define IDM_TXD_U8(descriptor, offset) \
    (*(uint8_t *)((uint8_t *)(descriptor) + (offset)))
#define IDM_SKB_U32(skb, offset) \
    (*(uint32_t *)((uint8_t *)(skb) + (offset)))
#define IDM_SKB_U8(skb, offset) \
    (*(uint8_t *)((uint8_t *)(skb) + (offset)))
#define IDM_SKB_PTR(skb, offset) \
    (*(void **)((uint8_t *)(skb) + (offset)))

extern int printk(const char *format, ...);
extern int isCpuType_133(void);
extern int isCpuType_129(void);
extern int isCpuType_132(void);
extern uint32_t get_idm_reserved_size(void);
extern uint32_t get_idm_reserved_base(void);
extern unsigned int cpumask_next(unsigned int cpu, const void *mask);
extern void *__kmalloc(size_t size, unsigned int flags);
extern void *kmem_cache_create(const char *name, size_t size, size_t align,
                                unsigned int flags, void *ctor);
extern void *kmem_cache_alloc(void *cache, unsigned int flags);
extern void *memset(void *destination, int value, size_t size);
extern void *memcpy(void *destination, const void *source, size_t size);
extern void kfree(void *object);
extern void __dsb(unsigned int option);
extern int idm_fifo_in(unsigned int fifo, uintptr_t buffer);
extern int idm_rx_refill(uint32_t *entry, unsigned int jumbo);
int idm_cfg_int(void);
int cpu_register_netinfo(struct idm_net_info *info);
extern int cpu_net_init(void);
extern int request_threaded_irq(unsigned int irq, idm_irq_handler_t handler,
                                idm_irq_handler_t thread_handler,
                                unsigned long flags, const char *name,
                                void *dev_id);
extern int irq_set_affinity_hint(unsigned int irq, const void *mask);
extern int __printk_ratelimit(const char *function_name);
extern int idm_cpu_int(int irq, void *dev_id);
extern int idm_wifi_int(int irq, void *dev_id);
extern int idm_rls_int(int irq, void *dev_id);
extern int idm_all_int(int irq, void *dev_id);
extern void cpu_net_int(unsigned int source);
extern void idm_rx_refill_reuse(uint32_t old_buffer, int pool);
extern void *idm_alloc_buf(unsigned int pool);
extern uintptr_t read_tpidr_el1(void);
extern uintptr_t read_sp_el0(void);
extern void *net_alloc_kmem(void);
extern void net_free_kmem(void *buffer);
extern void kmem_cache_free(void *cache, void *object);
extern void __local_bh_enable_ip(uintptr_t ip, unsigned int offset);
uintptr_t __my_cpu_offset_0(void);
uint64_t virt_to_phys_0(const void *address);
extern void data_padding(zte_skb_t *skb);
uint32_t __fswab32_1(uint32_t value);
extern void idm_rx_refill_flush(void);
extern unsigned long read_daif(void);
extern void daifset_irq(void);
extern void write_daif(unsigned long flags);
unsigned long arch_local_irq_save_1(void);
void do_raw_spin_lock_flags_idm_lock_int(void);
extern void queued_spin_lock_slowpath(uint32_t *lock, uint32_t observed,
                                      uintptr_t argument_2,
                                      unsigned int argument_3);
void arch_local_irq_restore_2(unsigned long flags);

extern volatile uint8_t *nppt_base;
extern uint64_t memstart_addr;
extern uint64_t kimage_voffset;
extern uint64_t vabits_actual;
extern uint32_t nr_cpu_ids;
extern const void *__cpu_possible_mask;
extern uintptr_t __per_cpu_offset[];
extern uint8_t idm_free_data[];
extern uint64_t cpu_bit_bitmap[];
extern uint8_t cpu_number[];

extern uint32_t uIdm_Int_Rls;
extern uint32_t uIDM_RX_NORMAL_BP_NUM;
extern uint32_t uIDM_RX_JUMBO_BP_NUM;
extern uint32_t uIDM_TX_JUMBO_BP_RETRV_NUM;
extern uint32_t uIDM_TX_NORMAL_BP_RETRV_NUM;
extern uint32_t uIDM_TX_EXTRAL_BP_RETRV_NUM;
extern uint32_t uIDM_TX_QUEUE_DESC_DEPTH;
extern uint32_t uIDM_RX_QUEUE_DESC_DEPTH;
extern uint32_t uIDM_TX_NORMAL_BP_NUM;
extern uint32_t uIDM_RX_NORMAL_BUFFER_NUM;
extern uint32_t uIDM_RX_JUMBO_BUFFER_NUM;
extern uint32_t uIDM_TX_JUMBO_BP_NUM;
extern uint32_t uNORMAL_BP_SIZE;
extern uint32_t uJUMBO_BP_SIZE;
extern uint32_t uSKB_SHAREDINFO_SIZE;
extern uint32_t uIDM_BP_CFG_UNIT;
extern uint32_t uNPPT_IDM_RX_QUEUE_NUM;
extern uint32_t uNPPT_IDM_TX_QUEUE_NUM;
extern uint32_t uNPPT_IDM_DESC_MODE;
extern uint32_t uIDM_RX_CFG_DEPTH;
extern uint32_t uIDM_TX_CFG_DEPTH;
extern uint32_t uBP_BUFFER_OFFSET;

extern uint32_t idm_reserved_base;
extern uint32_t idm_lock_int;
extern uint32_t idm_refill_lock;
extern uint32_t idm_lock_cfg;
extern struct idm_rx_queue idm_rx_q[IDM_RX_QUEUE_COUNT];
extern struct idm_tx_queue idm_tx_q[IDM_TX_QUEUE_COUNT];
extern struct idm_fifo idm_fifo[2];
extern uint32_t idm_fifo_empty[];
extern uint32_t idm_fifo_out_ncnt;
extern uint32_t idm_fifo_out_cnt[];
extern uint32_t idm_fifo_in_cnt[];
extern uint32_t idm_fifo_in_ncnt;
extern uint32_t idm_debug_cnt;
extern uint32_t dword_288A0;
extern void **buf_tq[IDM_TX_QUEUE_COUNT];
extern void *idm_buf_cache;
extern void *kmem_buf_cache;
extern void *idm_jbuf_cache;
extern uintptr_t idm_normal_buf_base;
extern uintptr_t idm_jumbo_buf_base;
extern uintptr_t rx_buf_ring;
extern uintptr_t rx_jbuf_ring;
extern uint32_t idm_refill_index;
extern uint32_t idm_jumbo_refill_index;
extern uintptr_t tx_cmpl_ring_base[3];
extern uintptr_t idm_txq_reg;
extern uintptr_t idm_txq_reg_word_0;
extern uintptr_t idm_txq_reg_word_1;
extern uintptr_t idm_txq_reg_word_2;
extern uint32_t idm_int_mask;
extern struct idm_net_info idm_info;
extern void *idm_ops;
extern idm_free_skb_data_t pp_free_skb_data;
void idm_free_skb_data(zte_skb_t *skb);
extern int g_idm_irq[4];
extern uint32_t g_idm_irq_to_cpu;
extern uint32_t eth_xmit_mode;
extern uint32_t idm_status[];
extern struct idm_refill_staging idm_refill_data[];
extern void *cpu_net_ops;
extern uint32_t cpu_net_info_word_0;
extern uint32_t cpu_net_info_word_4;
extern uint32_t cpu_net_info_word_8;
extern uint32_t cpu_net_info_word_c;
extern int32_t net_tx_debug;
extern int32_t net_rx_debug;
extern int32_t idm_tx_debug;
extern int32_t idm_rx_debug;
extern int32_t np1_trap_debug;
extern int32_t omci_tx_debug;
extern int32_t omci_rx_debug;
extern uint32_t last_extral_cnt;
extern uint32_t last_normal_cnt;
extern uint32_t last_jumbo_cnt;
extern uint32_t last_normal_idx;
extern uint32_t last_jumbo_idx;
extern uint32_t last_extral_idx;

/* Adjacent 32-bit globals at 0x264f8 through 0x26504. */
extern uint32_t idm_irq_target_cpu[4];

int idm_init(void)
{
    uint32_t reserved_size;
    uint32_t reserved_base;
    uint32_t descriptor_bytes;
    uint32_t free_ring_bytes;
    uint32_t buffer_data_phys;
    uint32_t normal_slots;
    uint32_t jumbo_slots;
    uint32_t normal_stride;
    uint32_t jumbo_stride;
    uint32_t normal_fifo_size;
    uint32_t jumbo_fifo_size;
    uint32_t free_ring_phys;
    uint32_t irq_mask;
    uint32_t refill_remaining;
    uint32_t refill_batch;
    uint32_t i;
    uint64_t required_size;
    uintptr_t normal_buffer_va;
    uintptr_t jumbo_buffer_va;
    uint8_t *descriptor_va;
    int uses_13x_idm;

    uses_13x_idm = isCpuType_133() || isCpuType_129();
    if (uses_13x_idm)
        uIdm_Int_Rls = 0x07000000U;
    if (isCpuType_132())
        uIdm_Int_Rls = 0x03000000U;

    idm_reserved_base = 0;
    reserved_size = get_idm_reserved_size();
    reserved_base = get_idm_reserved_base();
    if (reserved_base == 0) {
        printk("alloc idm reserved mem failed\n");
        return -1;
    }

    descriptor_bytes = (uIDM_TX_QUEUE_DESC_DEPTH +
                        6U * uIDM_RX_QUEUE_DESC_DEPTH) << 7;
    free_ring_bytes = 4U * (uIDM_RX_NORMAL_BP_NUM +
                            uIDM_RX_JUMBO_BP_NUM +
                            uIDM_TX_JUMBO_BP_RETRV_NUM +
                            uIDM_TX_NORMAL_BP_RETRV_NUM +
                            uIDM_TX_EXTRAL_BP_RETRV_NUM);
    normal_slots = uIDM_TX_NORMAL_BP_NUM + uIDM_RX_NORMAL_BUFFER_NUM;
    jumbo_slots = uIDM_RX_JUMBO_BUFFER_NUM + uIDM_TX_JUMBO_BP_NUM;
    normal_stride = uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE + 320U;
    jumbo_stride = uJUMBO_BP_SIZE - uSKB_SHAREDINFO_SIZE + 320U;

    /* The binary reserves an additional unexplained 0x800-byte margin. */
    required_size = 0x800U + free_ring_bytes + descriptor_bytes + 0x800U +
                    (uint64_t)normal_slots * normal_stride +
                    (uint64_t)jumbo_slots * jumbo_stride;
    printk("alloc idm reserved mem size %x/%x\n", reserved_size,
           (uint32_t)required_size);
    if (required_size > reserved_size)
        return -1;

    idm_reserved_base = reserved_base;
    printk("idm_reserved_base is %x\n", reserved_base);

    for (i = cpumask_next((uint32_t)-1, __cpu_possible_mask);
         i < nr_cpu_ids;
         i = cpumask_next(i, __cpu_possible_mask)) {
        *(uint32_t *)(idm_free_data + __per_cpu_offset[i] + 0x200) = 0;
        *(uint32_t *)(idm_free_data + __per_cpu_offset[i] + 0x204) = 0;
    }
    idm_lock_int = 0;
    idm_refill_lock = 0;
    idm_lock_cfg = 0;

    /* Raw IDM register program at nppt_base + 0x280000. */
    IDM_REG32(0x000) |= 0x000f0000U;
    IDM_REG32(0x000) = (IDM_REG32(0x000) & 0xf00fffffU) | 0x00f00000U;
    IDM_REG32(0x000) |= 0x00003000U;
    IDM_REG32(0x054) = 0x06060606U;
    IDM_REG32(0x058) = 0x00060606U;
    IDM_REG32(0x05c) = 0x07070707U;
    IDM_REG32(0x060) = 0x07070707U;
    if (uses_13x_idm)
        IDM_REG32(0x5c0) = 7;
    IDM_REG32(0x090) = 20;
    IDM_REG32(0x094) = 1;
    IDM_REG32(0x3fc) = 0x0f49U;
    IDM_REG32(0x074) = 0x210U;
    for (i = 0x014; i != 0x038; i += 4)
        IDM_REG32(i) = 0x00800080U;
    IDM_REG32(0x038) = 50000;
    IDM_REG32(0x010) = 128;

    if (uNPPT_IDM_RX_QUEUE_NUM != IDM_RX_QUEUE_COUNT ||
        uNPPT_IDM_TX_QUEUE_NUM != IDM_TX_QUEUE_COUNT) {
        printk("idm_buffer_init failed\n");
        return -1;
    }

    for (i = 0; i < IDM_TX_QUEUE_COUNT; ++i) {
        buf_tq[i] = (void **)__kmalloc(8U * uIDM_TX_QUEUE_DESC_DEPTH,
                                       IDM_KMALLOC_FLAGS);
        if (buf_tq[i] == 0) {
            printk("idm_buffer_init failed\n");
            return -1;
        }
    }

    idm_fifo[0].lock_word = 0;
    idm_fifo[1].lock_word = 0;
    idm_fifo[0].buffer =
        (void **)__kmalloc(8U * normal_slots, IDM_KMALLOC_FLAGS);
    idm_fifo[1].buffer =
        (void **)__kmalloc(8U * jumbo_slots, IDM_KMALLOC_FLAGS);
    if (idm_fifo[0].buffer == 0) {
        printk("idm_buffer_init failed\n");
        return -1;
    }

    normal_fifo_size = 1;
    while (normal_fifo_size < normal_slots)
        normal_fifo_size <<= 1;
    jumbo_fifo_size = 1;
    while (jumbo_fifo_size < jumbo_slots)
        jumbo_fifo_size <<= 1;
    idm_fifo[0].mask = normal_fifo_size - 1;
    idm_fifo[1].mask = jumbo_fifo_size - 1;
    idm_fifo[0].buffer_data_length = uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE;
    idm_fifo[1].buffer_data_length = uJUMBO_BP_SIZE - uSKB_SHAREDINFO_SIZE;

    buffer_data_phys = reserved_base + descriptor_bytes + free_ring_bytes +
                       0x800U;
    normal_buffer_va = IDM_LINEAR_ADDRESS(buffer_data_phys);
    jumbo_buffer_va = normal_buffer_va + (uintptr_t)normal_slots * normal_stride;
    idm_normal_buf_base = normal_buffer_va;
    idm_jumbo_buf_base = jumbo_buffer_va;

    for (i = 0; i < normal_slots; ++i)
        idm_fifo_in(0, normal_buffer_va + (uintptr_t)i * normal_stride);
    for (i = 0; i < jumbo_slots; ++i)
        idm_fifo_in(1, jumbo_buffer_va + (uintptr_t)i * jumbo_stride);

    printk("idm buf cache len %d\n",
           ((uNORMAL_BP_SIZE + 63U - uSKB_SHAREDINFO_SIZE) & ~63U) + 384U);
    idm_buf_cache = kmem_cache_create(
        "idm buf cache",
        ((uNORMAL_BP_SIZE + 63U - uSKB_SHAREDINFO_SIZE) & ~63U) + 384U,
        0, IDM_CACHE_FLAGS, 0);
    kmem_buf_cache = idm_buf_cache;
    printk("idm jbuf cache len %d\n",
           ((uJUMBO_BP_SIZE + 63U - uSKB_SHAREDINFO_SIZE) & ~63U) + 384U);
    idm_jbuf_cache = kmem_cache_create(
        "idm jbuf cache",
        ((uJUMBO_BP_SIZE + 63U - uSKB_SHAREDINFO_SIZE) & ~63U) + 384U,
        0, IDM_CACHE_FLAGS, 0);

    IDM_REG32(0x124) = 0;
    IDM_REG32(0x10c) =
        (((uIDM_RX_NORMAL_BP_NUM / uIDM_BP_CFG_UNIT - 1U) << 21) |
         ((uIDM_TX_NORMAL_BP_RETRV_NUM / uIDM_BP_CFG_UNIT - 1U) << 10) |
         0x100U);
    IDM_REG32(0x110) =
        (((uIDM_RX_JUMBO_BP_NUM / uIDM_BP_CFG_UNIT - 1U) << 21) |
         ((uIDM_TX_JUMBO_BP_RETRV_NUM / uIDM_BP_CFG_UNIT - 1U) << 10) |
         0x100U);
    if (uses_13x_idm)
        IDM_REG32(0x40c) =
            ((uIDM_TX_EXTRAL_BP_RETRV_NUM / uIDM_BP_CFG_UNIT - 1U) << 10) |
            0x100U;

    free_ring_phys = reserved_base + descriptor_bytes;
    IDM_REG32(0x104) = free_ring_phys;
    free_ring_phys += 4U * uIDM_RX_NORMAL_BP_NUM;
    IDM_REG32(0x108) = free_ring_phys;
    free_ring_phys += 4U * uIDM_RX_JUMBO_BP_NUM;
    IDM_REG32(0x118) = free_ring_phys;
    free_ring_phys += 4U * uIDM_TX_NORMAL_BP_RETRV_NUM;
    IDM_REG32(0x11c) = free_ring_phys;
    if (uses_13x_idm)
        IDM_REG32(0x408) = reserved_base + descriptor_bytes +
                            4U * (uIDM_RX_NORMAL_BP_NUM +
                                  uIDM_RX_JUMBO_BP_NUM +
                                  uIDM_TX_NORMAL_BP_RETRV_NUM +
                                  uIDM_TX_JUMBO_BP_RETRV_NUM);

    rx_buf_ring = IDM_LINEAR_ADDRESS(reserved_base + descriptor_bytes);
    printk("rx ring %x/%x @ %p, jumbo rx ring %x/%x\n",
           uIDM_RX_NORMAL_BUFFER_NUM, uIDM_RX_NORMAL_BP_NUM,
           (void *)rx_buf_ring, uIDM_RX_JUMBO_BUFFER_NUM,
           uIDM_RX_JUMBO_BP_NUM);
    for (i = 0; i < uIDM_RX_NORMAL_BUFFER_NUM; ++i) {
        if (idm_rx_refill((uint32_t *)rx_buf_ring + i, 0) < 0) {
            printk("idm_buffer_init failed\n");
            return -1;
        }
    }
    for (refill_remaining = uIDM_RX_NORMAL_BUFFER_NUM;
         refill_remaining != 0;
         refill_remaining -= refill_batch) {
        refill_batch = refill_remaining > IDM_REFILL_BATCH ?
                       IDM_REFILL_BATCH : refill_remaining;
        __dsb(0x0e);
        IDM_REG32(0x100) = refill_batch;
    }

    rx_jbuf_ring = IDM_LINEAR_ADDRESS(reserved_base + descriptor_bytes +
                                       4U * uIDM_RX_NORMAL_BP_NUM);
    for (i = 0; i < uIDM_RX_JUMBO_BUFFER_NUM; ++i) {
        if (idm_rx_refill((uint32_t *)rx_jbuf_ring + i, 1) < 0) {
            printk("idm_buffer_init failed\n");
            return -1;
        }
    }
    for (refill_remaining = uIDM_RX_JUMBO_BUFFER_NUM;
         refill_remaining != 0;
         refill_remaining -= refill_batch) {
        refill_batch = refill_remaining > IDM_REFILL_BATCH ?
                       IDM_REFILL_BATCH : refill_remaining;
        __dsb(0x0e);
        IDM_REG32(0x100) = refill_batch << 16;
    }
    idm_refill_index = uIDM_RX_NORMAL_BUFFER_NUM;
    idm_jumbo_refill_index = uIDM_RX_JUMBO_BUFFER_NUM;

    tx_cmpl_ring_base[0] = IDM_LINEAR_ADDRESS(
        reserved_base + descriptor_bytes +
        4U * (uIDM_RX_NORMAL_BP_NUM + uIDM_RX_JUMBO_BP_NUM));
    tx_cmpl_ring_base[1] = IDM_LINEAR_ADDRESS(
        reserved_base + descriptor_bytes +
        4U * (uIDM_RX_NORMAL_BP_NUM + uIDM_RX_JUMBO_BP_NUM +
              uIDM_TX_NORMAL_BP_RETRV_NUM));
    if (uses_13x_idm)
        tx_cmpl_ring_base[2] =
            IDM_LINEAR_ADDRESS(reserved_base + descriptor_bytes +
                               4U * (uIDM_RX_NORMAL_BP_NUM +
                                     uIDM_RX_JUMBO_BP_NUM +
                                     uIDM_TX_NORMAL_BP_RETRV_NUM +
                                     uIDM_TX_JUMBO_BP_RETRV_NUM));

    IDM_REG32(0x0c0) = uNPPT_IDM_DESC_MODE;
    IDM_REG32(0x000) = (IDM_REG32(0x000) & 0x8fffffffU) |
                       (uIDM_RX_CFG_DEPTH << 28);
    IDM_REG32(0x070) = uIDM_RX_QUEUE_DESC_DEPTH - 1U;
    IDM_REG32(0x008) = reserved_base;
    IDM_REG32(0x00c) = uIDM_TX_CFG_DEPTH << 16;
    IDM_REG32(0x004) = reserved_base +
                        IDM_RX_QUEUE_COUNT * 32U * uIDM_RX_QUEUE_DESC_DEPTH;

    descriptor_va = (uint8_t *)IDM_LINEAR_ADDRESS(reserved_base);
    memset(descriptor_va, 0, IDM_RX_QUEUE_COUNT * 32U *
                             uIDM_RX_QUEUE_DESC_DEPTH);
    for (i = 0; i < IDM_RX_QUEUE_COUNT; ++i) {
        idm_rx_q[i].descriptor_base = descriptor_va;
        descriptor_va += 32U * uIDM_RX_QUEUE_DESC_DEPTH;
        idm_rx_q[i].producer = 0;
        idm_rx_q[i].depth = uIDM_RX_QUEUE_DESC_DEPTH;
    }

    descriptor_va = (uint8_t *)IDM_LINEAR_ADDRESS(
        reserved_base + IDM_RX_QUEUE_COUNT * 32U * uIDM_RX_QUEUE_DESC_DEPTH);
    memset(descriptor_va, 0, uIDM_TX_QUEUE_DESC_DEPTH << 7);
    for (i = 0; i < IDM_TX_QUEUE_COUNT; ++i) {
        idm_tx_q[i].descriptor_base = descriptor_va;
        idm_tx_q[i].buffer_table = buf_tq[i];
        idm_tx_q[i].word_10 = 0;
        idm_tx_q[i].word_14 = 0;
        idm_tx_q[i].word_18 = 0;
        idm_tx_q[i].queue_index = i;
        idm_tx_q[i].depth = uIDM_TX_QUEUE_DESC_DEPTH;
        idm_tx_q[i].word_24 = 0;
        descriptor_va += 32U * uIDM_TX_QUEUE_DESC_DEPTH;
    }

    idm_txq_reg = (uintptr_t)(nppt_base + IDM_REG_BASE + 0x080U);
    idm_txq_reg_word_0 = (uintptr_t)(nppt_base + IDM_REG_BASE + 0x0a0U);
    idm_txq_reg_word_1 = (uintptr_t)(nppt_base + IDM_REG_BASE + 0x0a4U);
    idm_txq_reg_word_2 = (uintptr_t)(nppt_base + IDM_REG_BASE + 0x0a8U);
    irq_mask = uIdm_Int_Rls | 0x00ffffffU;
    IDM_REG32(0x040) = irq_mask;
    idm_int_mask = irq_mask;
    idm_info.word_0 = 65023U;
    idm_info.word_4 = 0x00ff0000U;
    idm_info.word_8 = 512U;
    idm_info.ops = idm_ops;

    if (idm_cfg_int() < 0)
        return -1;

    cpu_register_netinfo(&idm_info);
    pp_free_skb_data = idm_free_skb_data;
    IDM_REG32(0x06c) = uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE -
                        uBP_BUFFER_OFFSET - 63U;
    cpu_net_init();
    return 0;
}

int idm_cfg_int(void)
{
    int ret;

    IDM_REG32(0x044) = idm_info.word_0;
    IDM_REG32(0x048) = idm_info.word_4;
    IDM_REG32(0x04c) = idm_info.word_c;
    IDM_REG32(0x050) = idm_info.word_8;

    ret = request_threaded_irq((unsigned int)g_idm_irq[0], idm_cpu_int, 0, 0,
                               "cpu", 0);
    if (ret < 0) {
        printk("request cpu irq %d failed\n", g_idm_irq[0]);
        return ret;
    }

    ret = request_threaded_irq((unsigned int)g_idm_irq[1], idm_wifi_int, 0, 0,
                               "idm", 0);
    if (ret < 0) {
        printk("request idm irq %d failed\n", g_idm_irq[1]);
        return ret;
    }

    ret = request_threaded_irq((unsigned int)g_idm_irq[2], idm_rls_int, 0, 0,
                               "buf_rls", 0);
    if (ret < 0) {
        printk("request rls irq %d failed\n", g_idm_irq[2]);
        return ret;
    }

    ret = request_threaded_irq((unsigned int)g_idm_irq[3], idm_all_int, 0, 0,
                               "localtest", 0);
    if (ret < 0) {
        printk("request local speedtest irq %d failed\n", g_idm_irq[3]);
        return ret;
    }

    if (nr_cpu_ids == 4) {
        idm_irq_target_cpu[0] = 1;
        irq_set_affinity_hint((unsigned int)g_idm_irq[0],
                              IDM_AFFINITY_HINT(1));
        irq_set_affinity_hint((unsigned int)g_idm_irq[1],
                              IDM_AFFINITY_HINT(2));
        idm_irq_target_cpu[1] = 2;
        irq_set_affinity_hint((unsigned int)g_idm_irq[2],
                              IDM_AFFINITY_HINT(2));
        idm_irq_target_cpu[2] = 2;
        irq_set_affinity_hint((unsigned int)g_idm_irq[3],
                              IDM_AFFINITY_HINT(3));
        idm_irq_target_cpu[3] = 3;
    } else if (nr_cpu_ids == 2) {
        irq_set_affinity_hint((unsigned int)g_idm_irq[0],
                              IDM_AFFINITY_HINT(0));
        idm_irq_target_cpu[0] = 0;
        irq_set_affinity_hint((unsigned int)g_idm_irq[1],
                              IDM_AFFINITY_HINT(1));
        idm_irq_target_cpu[1] = 1;
        irq_set_affinity_hint((unsigned int)g_idm_irq[2],
                              IDM_AFFINITY_HINT(0));
        idm_irq_target_cpu[2] = 0;
        irq_set_affinity_hint((unsigned int)g_idm_irq[3],
                              IDM_AFFINITY_HINT(1));
        idm_irq_target_cpu[3] = 1;
    } else if (__printk_ratelimit("idm_cfg_int")) {
        printk("idm err: NR_CPUS %u\n", 2U);
    }

    return 0;
}

void idm_int_enable(uint32_t bits)
{
    unsigned long flags;

    flags = arch_local_irq_save_1();
    do_raw_spin_lock_flags_idm_lock_int();
    idm_int_mask &= ~bits;
    IDM_REG32(0x040) = idm_int_mask;

    /* The binary releases the fixed lock with an STLRB zero store. */
    *(volatile uint8_t *)&idm_lock_int = 0;
    arch_local_irq_restore_2(flags);
}

void idm_int_disable(uint32_t bits)
{
    unsigned long flags;

    flags = arch_local_irq_save_1();
    do_raw_spin_lock_flags_idm_lock_int();
    idm_int_mask |= bits;
    IDM_REG32(0x040) = idm_int_mask;

    *(volatile uint8_t *)&idm_lock_int = 0;
    arch_local_irq_restore_2(flags);
}

int idm_cpu_int(int irq, void *dev_id)
{
    (void)irq;
    (void)dev_id;

    idm_int_disable(idm_info.word_0);
    cpu_net_int(0);
    return 1;
}

int idm_wifi_int(int irq, void *dev_id)
{
    (void)irq;
    (void)dev_id;

    idm_int_disable(idm_info.word_4);
    cpu_net_int(1);
    return 1;
}

int idm_rls_int(int irq, void *dev_id)
{
    (void)irq;
    (void)dev_id;

    idm_int_disable(idm_info.word_c);
    cpu_net_int(3);
    return 1;
}

int idm_all_int(int irq, void *dev_id)
{
    (void)irq;
    (void)dev_id;

    idm_int_disable(idm_info.word_8);
    cpu_net_int(2);
    return 1;
}

struct idm_rx_queue *idm_get_cpu_rx_qc(uint32_t index)
{
    return &idm_rx_q[index];
}

struct idm_tx_queue *idm_get_cpu_tx_q(uint32_t index)
{
    if (index >= IDM_TX_QUEUE_COUNT)
        return 0;

    return &idm_tx_q[index];
}

uint32_t idm_get_cpu_rx_cnt(uint32_t index)
{
    uint32_t first;
    uint32_t second;
    uint32_t group;

    if (index > 7)
        return IDM_REG32(4U * ((index + 0x31U) & 0x3fffffffU));

    group = index >> 1;
    first = IDM_REG32(4U * (group + 0x31U));
    second = IDM_REG32(4U * (group + 0x35U));
    if ((index & 1U) != 0)
        return (second & 0xffff0000U) | (first >> 16);

    return (second << 16) | (first & 0x0000ffffU);
}

uint16_t idm_get_tx_done(uint32_t index)
{
    uint32_t value;

    if (index == 0)
        value = IDM_REG32(0x084);
    else
        value = IDM_REG32(4U * ((index + 0x2aU) & 0x3fffffffU));

    return (uint16_t)value;
}

uint32_t idm_get_reorder_rls(unsigned int queue)
{
    if (queue == 0U)
        return IDM_REG32(0x08cU);
    if (queue == 1U)
        return IDM_REG32(0x0f8U);
    if (queue == 2U &&
        (isCpuType_133() == 1 || isCpuType_129() == 1))
        return IDM_REG32(0x41cU);

    return 0;
}

int idm_rx_refill0(uint32_t old_buffer, int pool, int reuse)
{
    struct idm_refill_staging *staging;
    uint32_t buffer_data_phys;
    uint32_t cpu;
    uint32_t physical;
    uint32_t slot;
    void *buffer;

    if (reuse) {
        idm_rx_refill_reuse(old_buffer, pool);
        return 0;
    }

    buffer = idm_alloc_buf((unsigned int)pool);
    if (buffer == 0) {
        if (old_buffer != 0)
            idm_rx_refill_reuse(old_buffer, pool);
        return -1;
    }

    cpu = *(uint32_t *)(cpu_number + __my_cpu_offset_0());
    buffer_data_phys = idm_reserved_base + 0x800U +
                       4U * (uIDM_RX_NORMAL_BP_NUM +
                             uIDM_RX_JUMBO_BP_NUM +
                             uIDM_TX_JUMBO_BP_RETRV_NUM +
                             uIDM_TX_NORMAL_BP_RETRV_NUM +
                             uIDM_TX_EXTRAL_BP_RETRV_NUM) +
                       ((uIDM_TX_QUEUE_DESC_DEPTH +
                         6U * uIDM_RX_QUEUE_DESC_DEPTH) << 7);
    ++idm_status[pool + 2U * cpu +
                 (old_buffer >= buffer_data_phys ? 4U : 12U)];

    staging = &idm_refill_data[cpu];
    physical = __fswab32_1(virt_to_phys_0(
        (uint8_t *)buffer + uBP_BUFFER_OFFSET + 64U));
    slot = staging->count[pool]++;
    staging->entries[pool][slot] = physical;
    if (staging->count[pool] > 31U)
        idm_rx_refill_flush();

    return 0;
}

int cpu_register_netinfo(struct idm_net_info *info)
{
    cpu_net_ops = info->ops;
    cpu_net_info_word_0 = info->word_0;
    cpu_net_info_word_4 = info->word_4;
    cpu_net_info_word_8 = info->word_8;
    cpu_net_info_word_c = info->word_c;
    return (int)info->word_c;
}

int idm_cpu_tx(zte_skb_t *skb, void *descriptor)
{
    unsigned int length;
    unsigned int port;

    IDM_TXD_U32(descriptor, 0) = virt_to_phys_0(IDM_SKB_PTR(skb, 0x130));
    if (IDM_SKB_U32(skb, 0xa8) <= 59U)
        data_padding(skb);

    length = IDM_SKB_U32(skb, 0xa8);
    if (length > 0x3fffU) {
        /* The binary rate-limits a diagnostic and calls dump_stack here. */
        IDM_SKB_U32(skb, 0xa8) = 60;
        length = 60;
    }

    IDM_TXD_U32(descriptor, 4) = 0;
    IDM_TXD_U32(descriptor, 8) = 0x00400000U;
    IDM_TXD_U16(descriptor, 4) =
        (IDM_TXD_U16(descriptor, 4) & 0x8001U) |
        ((length & 0x3fffU) << 1);

    port = IDM_SKB_U8(skb, 0x108);
    if (port > 14U) {
        IDM_TXD_U8(descriptor, 7) =
            (IDM_TXD_U8(descriptor, 7) & 0xc0U) | 0x20U;
        IDM_TXD_U8(descriptor, 10) =
            (IDM_TXD_U8(descriptor, 10) & 0xc0U) | (port & 0x0fU);
    } else {
        IDM_TXD_U8(descriptor, 7) =
            (IDM_TXD_U8(descriptor, 7) & 0xc0U) | 0x0fU;
        IDM_TXD_U8(descriptor, 10) =
            (IDM_TXD_U8(descriptor, 10) & 0xc0U) | port;
    }

    __dsb(0xe);
    *(volatile uint32_t *)idm_txq_reg_word_0 = 0x20000U;
    return 0;
}

int idm_omci_tx(zte_skb_t *skb, void *descriptor)
{
    unsigned int length;

    IDM_TXD_U32(descriptor, 0) = virt_to_phys_0(IDM_SKB_PTR(skb, 0x130));
    if (IDM_SKB_U32(skb, 0xa8) <= 14U)
        IDM_SKB_U32(skb, 0xa8) = 15;

    length = IDM_SKB_U32(skb, 0xa8);
    IDM_TXD_U32(descriptor, 4) = 0;
    IDM_TXD_U32(descriptor, 8) = 0x00400001U;
    IDM_TXD_U16(descriptor, 4) =
        (IDM_TXD_U16(descriptor, 4) & 0x8001U) |
        ((length & 0x3fffU) << 1);
    IDM_TXD_U8(descriptor, 7) =
        (IDM_TXD_U8(descriptor, 7) & 0xc0U) | 0x0fU;
    IDM_TXD_U8(descriptor, 10) =
        (IDM_TXD_U8(descriptor, 10) & 0xc0U) | IDM_SKB_U8(skb, 0x108);

    __dsb(0xe);
    *(volatile uint32_t *)idm_txq_reg = 0x20000U;
    return 0;
}

int idm_wifi_tx(zte_skb_t *skb, void *descriptor)
{
    unsigned int length;
    unsigned int port;

    IDM_TXD_U32(descriptor, 0) = virt_to_phys_0(IDM_SKB_PTR(skb, 0x130));
    if (IDM_SKB_U32(skb, 0xa8) <= 59U)
        data_padding(skb);

    length = IDM_SKB_U32(skb, 0xa8);
    IDM_TXD_U32(descriptor, 4) = 0;
    IDM_TXD_U32(descriptor, 8) = 0x00400000U;
    IDM_TXD_U16(descriptor, 4) =
        (IDM_TXD_U16(descriptor, 4) & 0x8001U) |
        ((length & 0x3fffU) << 1);

    port = IDM_SKB_U8(skb, 0x108) & 0x3fU;
    IDM_TXD_U8(descriptor, 7) =
        (IDM_TXD_U8(descriptor, 7) & 0xc0U) | port;
    IDM_TXD_U32(descriptor, 16) = port << 26;

    __dsb(0xe);
    *(volatile uint32_t *)idm_txq_reg_word_1 = 0x20000U;
    return 0;
}

int test_and_set_bit(unsigned int bit, volatile unsigned long *address)
{
    unsigned long mask = 1UL << (bit & 63U);
    volatile unsigned long *word = address + (bit >> 6);
    unsigned long previous;

    if ((*word & mask) != 0)
        return 1;

    __builtin_prefetch((const void *)word, 1, 1);
    for (;;) {
        unsigned long desired;

        previous = __atomic_load_n(word, __ATOMIC_RELAXED);
        desired = previous | mask;
        if (__atomic_compare_exchange_n(word, &previous, desired, 0,
                                        __ATOMIC_RELEASE,
                                        __ATOMIC_RELAXED))
            break;
    }

    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    return (previous & mask) != 0;
}

uint32_t __fswab32_1(uint32_t value)
{
    return ((value & 0x000000ffU) << 24) |
           ((value & 0x0000ff00U) << 8) |
           ((value & 0x00ff0000U) >> 8) |
           ((value & 0xff000000U) >> 24);
}

uint64_t virt_to_phys_0(const void *address)
{
    uint64_t virtual_address = (uint64_t)(uintptr_t)address;
    uint32_t va_bits = (uint32_t)vabits_actual;
    uint64_t direct_map_limit =
        0x8000000000ULL - (1ULL << (va_bits - 1U));

    if ((virtual_address ^ 0xffffff8000000000ULL) >= direct_map_limit)
        return virtual_address - kimage_voffset;

    return (virtual_address & 0x7fffffffffULL) + memstart_addr;
}

uintptr_t __my_cpu_offset_0(void)
{
    return read_tpidr_el1();
}

unsigned long arch_local_irq_save_1(void)
{
    unsigned long flags = read_daif();

    if ((flags & 0x80U) == 0)
        daifset_irq();
    return flags;
}

void arch_local_irq_restore_2(unsigned long flags)
{
    write_daif(flags);
}

void idm_rx_update(uint32_t queue, uint32_t count, uint32_t jumbo_count)
{
    __dsb(0xe);
    IDM_REG32(0x088U) = count | (queue << 12);
    IDM_REG32(0x100U) = (count - jumbo_count) | (jumbo_count << 16);
}

int idm_rx_test(void)
{
    return 0;
}

void idm_recv_debug_set(void)
{
    /* Exported compatibility stub; the machine body is only RET. */
}

void idm_tx_debug_set(int32_t value)
{
    net_tx_debug = value;
}

void idm_rx_debug_set(int32_t value)
{
    net_rx_debug = value;
}

void idm_wifi_tx_debug_set(int32_t value)
{
    idm_tx_debug = value;
}

void idm_wifi_rx_debug_set(int32_t value)
{
    idm_rx_debug = value;
    np1_trap_debug = value;
}

void idm_omci_tx_debug_set(int32_t value)
{
    omci_tx_debug = value;
}

void idm_omci_rx_debug_set(int32_t value)
{
    omci_rx_debug = value;
}

int idm_set_smct_all_trap(uint32_t enable)
{
    IDM_REG32(0x000U) =
        (IDM_REG32(0x000U) & 0xffffbfffU) | ((enable & 1U) << 14);
    return 0;
}

void set_last_extral_cnt(uint32_t value)
{
    last_extral_cnt = value;
}

void set_last_normal_cnt(uint32_t value)
{
    last_normal_cnt = value;
}

void set_last_jumbo_cnt(uint32_t value)
{
    last_jumbo_cnt = value;
}

uint32_t get_last_buffer_idx(uint32_t pool)
{
    if (pool == 1U)
        return last_jumbo_idx;
    if (pool == 2U)
        return last_extral_idx;
    if (pool == 0U)
        return last_normal_idx;
    return 0;
}

void set_last_buffer_idx(uint32_t pool, uint32_t value)
{
    if (pool == 1U) {
        last_jumbo_idx = value;
        return;
    }
    if (pool == 2U) {
        last_extral_idx = value;
        return;
    }
    if (pool == 0U)
        last_normal_idx = value;
}

void idm_stat(void)
{
    uint32_t value;

    printk("  receive cpu pkt cnt from DMA(0x60):                          0x%x\n",
           IDM_REG32(0x180U));
    printk("  receive wifi pkt cnt from DMA(0x61):                         0x%x\n",
           IDM_REG32(0x184U));

    value = IDM_REG32(0x188U);
    printk("  receive cpu pkt with len = 0 cnt from DMA(16bit)(0x62):      0x%x\n",
           value >> 16);
    printk("  receive wifi pkt with len = 0 cnt from DMA(16bit)(0x62):     0x%x\n",
           value & 0xffffU);

    value = IDM_REG32(0x18cU);
    printk("  receive jumbo req cnt from RED(16bit)(0x63):                 0x%x\n",
           value >> 16);
    printk("  receive normal req cnt from RED(16bit)(0x63):                0x%x\n",
           value & 0xffffU);

    value = IDM_REG32(0x190U);
    printk("  send res req cnt to RED(16bit)(0x64):                        0x%x\n",
           value >> 16);
    printk("  send res buffer cnt to RED(16bit)(0x64):                     0x%x\n",
           value & 0xffffU);
    printk("  send cpu pkt cnt to SIPC(0x65):                              0x%x\n",
           IDM_REG32(0x194U));

    value = IDM_REG32(0x198U);
    printk("  send read req to AXI(16bit)(0x66):                           0x%x\n",
           value >> 16);
    printk("  receive read ack from AXI(16bit)(0x66):                      0x%x\n",
           value & 0xffffU);

    value = IDM_REG32(0x19cU);
    printk("  send write req to AXI(16bit)(0x67):                          0x%x\n",
           value >> 16);
    printk("  receive write ack from AXI(16bit)(0x67):                     0x%x\n",
           value & 0xffffU);

    value = IDM_REG32(0x1d8U);
    printk("  receive desc with pkt err cnt from cpu(16bit)(0x76):          0x%x\n",
           value >> 16);
    printk("  receive desc with pkt correct cnt from cpu(16bit)(0x76):      0x%x\n",
           value & 0xffffU);

    value = IDM_REG32(0x1e4U);
    printk("  rev desc req cnt(16bit)(0x79):                                0x%x\n",
           value >> 16);
    printk("  rev desc ack cnt(16bit)(0x79):                                0x%x\n",
           value & 0xffffU);

    value = IDM_REG32(0x1e8U);
    printk("  rev data req cnt(16bit)(0x7a):                                0x%x\n",
           value >> 16);
    printk("  rev data ack cnt(16bit)(0x7a):                                0x%x\n",
           value & 0xffffU);

    value = IDM_REG32(0x1ecU);
    printk("  bp rd req cnt(16bit)(0x7b):                                   0x%x\n",
           value >> 16);
    printk("  bp rd ack cnt(16bit)(0x7b):                                   0x%x\n",
           value & 0xffffU);

    value = IDM_REG32(0x1f8U);
    printk("  bp wr req cnt(16bit)(0x7e):                                   0x%x\n",
           value >> 16);
    printk("  bp wr ack cnt(16bit)(0x7e):                                   0x%x\n",
           value & 0xffffU);

    printk("  receive pkt from smct(0x82):                                  0x%x\n",
           IDM_REG32(0x208U));
    printk("  send to reorder cnt(0x83):                                    0x%x\n",
           IDM_REG32(0x20cU));
    printk("  receive from reorder(exp proc) cnt(0x84):                     0x%x\n",
           IDM_REG32(0x210U));
    printk("  send to sipc and need recv( reorder exp proc->np2) cnt(0x85): 0x%x\n",
           IDM_REG32(0x214U));
    printk("  send to sipc(np1->np2) cnt(0x86):                             0x%x\n",
           IDM_REG32(0x218U));
    printk("  send to sipc insert(reorder exp proc->np2) cnt(0x87):         0x%x\n",
           IDM_REG32(0x21cU));
    printk("  send to sipc recv(reorder exp pro->np2) cnt(0x88):            0x%x\n",
           IDM_REG32(0x220U));

    value = IDM_REG32(0x224U);
    printk("  receive len err(np1->np2) cnt(16bit)(0x89):                   0x%x\n",
           value >> 16);
    printk("  receive len right(np1->np2) cnt(16bit)(0x89):                 0x%x\n",
           value & 0xffffU);

    value = IDM_REG32(0x228U);
    printk("  receive len err cnt from reorder(16bit)(0x8a):                0x%x\n",
           value >> 16);
    printk("  receive len right cnt from reorder(16bit)(0x8a):              0x%x\n",
           value & 0xffffU);
    printk("  idm dma debug state(0x81):                                    0x%x\n",
           IDM_REG32(0x204U));
}

void idm_debug_stat(void)
{
    uint32_t cpu;

    printk("\nnormal bp soft cnt:\n");
    for (cpu = 0; cpu != 2U; ++cpu) {
        printk("cpu %u:\n", cpu);
        printk("free_cnt: %u\n", idm_status[2U * cpu]);
        printk("alloc_cnt: %u\n", idm_status[2U * cpu + 4U]);
        printk("free_cnt1: %u\n", idm_status[2U * cpu + 8U]);
        printk("alloc_cnt1: %u\n", idm_status[2U * cpu + 12U]);
    }

    printk("jumbo bp soft cnt:\n");
    for (cpu = 0; cpu != 2U; ++cpu) {
        printk("cpu %u:\n", cpu);
        printk("free_cnt: %u\n", idm_status[2U * cpu + 1U]);
        printk("alloc_cnt: %u\n", idm_status[2U * cpu + 5U]);
        printk("free_cnt1: %u\n", idm_status[2U * cpu + 9U]);
        printk("alloc_cnt1: %u\n", idm_status[2U * cpu + 13U]);
    }

    printk("free_repeat: %u\n", idm_status[16]);
    printk("alloc_repeat: %u\n", idm_status[17]);
    idm_stat();
}

void idm_print_bppe(uint32_t address)
{
    printk("addr %x\n", address);
}

void data_padding(zte_skb_t *skb)
{
    uint32_t length = IDM_SKB_U32(skb, 0xa8);
    int32_t padding = 60 - (int32_t)length;
    int32_t tailroom = 0;

    if (IDM_SKB_U32(skb, 0xac) == 0) {
        tailroom = (int32_t)(IDM_SKB_U32(skb, 0x120) -
                             IDM_SKB_U32(skb, 0x11c));
    }
    if (padding <= tailroom) {
        memset((uint8_t *)IDM_SKB_PTR(skb, 0x130) + length, 0,
               (size_t)padding);
    }
    IDM_SKB_U32(skb, 0xa8) = 60U;
}

void idm_rls_update(uint32_t count_0, uint32_t count_1, uint32_t count_2)
{
    if (isCpuType_133() || isCpuType_129()) {
        IDM_REG32(0x07cU) = count_0 | (count_1 << 16);
        if (isCpuType_133() || isCpuType_129())
            IDM_REG32(0x418U) = count_2;
    }
}

void idm_cpu_nb_tx_update(uint32_t queue, uint32_t count)
{
    uint32_t value;

    __dsb(0xe);
    value = count << 17;
    if (queue == 0U)
        IDM_REG32(0x080U) = value;
    else
        IDM_REG32(4U * ((queue + 39U) & 0x3fffffffU)) = value;
}

int idm_get_smct_all_trap(uint32_t *mode)
{
    if (mode == 0) {
        if (__printk_ratelimit("idm_get_smct_all_trap"))
            printk("idm_get_smct_all_trap mode NULL\n");
        return -1;
    }

    *mode = (IDM_REG32(0x000U) >> 14) & 1U;
    return 0;
}

int get_order(unsigned long size)
{
    unsigned long pages = (size - 1UL) >> 12;

    if (pages == 0)
        return 0;
    return 64 - __builtin_clzl(pages);
}

void do_raw_spin_lock_1(uint32_t *lock)
{
    __builtin_prefetch((const void *)lock, 1, 1);

    for (;;) {
        uint32_t observed = 0;

        if (__atomic_compare_exchange_n(lock, &observed, 1U, 0,
                                        __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED)) {
            return;
        }
        if (observed != 0) {
            queued_spin_lock_slowpath(lock, observed, 0, 1);
            return;
        }
    }
}

void idm_rx_refill_flush(void)
{
    uint32_t cpu = *(uint32_t *)(cpu_number + __my_cpu_offset_0());
    struct idm_refill_staging *staging = &idm_refill_data[cpu];
    uint32_t index;
    uint32_t normal_count;
    uint32_t jumbo_count;
    uint32_t i;
    int wrote_normal = 0;

    do_raw_spin_lock_1(&idm_refill_lock);
    index = idm_refill_index;
    normal_count = staging->count[0];
    for (i = 0; i != normal_count; ++i) {
        ((uint32_t *)(uintptr_t)rx_buf_ring)[index++] = staging->entries[0][i];
        wrote_normal = 1;
        if (index >= uIDM_RX_NORMAL_BP_NUM)
            index = 0;
    }
    if (wrote_normal)
        idm_refill_index = index;

    jumbo_count = staging->count[1];
    staging->count[0] = 0;
    if (jumbo_count != 0) {
        index = idm_jumbo_refill_index;
        for (i = 0; i != jumbo_count; ++i) {
            ((uint32_t *)(uintptr_t)rx_jbuf_ring)[index++] =
                staging->entries[1][i];
            if (index >= uIDM_RX_NORMAL_BP_NUM)
                index = 0;
        }
        idm_jumbo_refill_index = index;
        staging->count[1] = 0;
    }

    __atomic_store_n((uint8_t *)&idm_refill_lock, 0, __ATOMIC_RELEASE);
}

void idm_rx_refill_reuse(uint32_t old_buffer, int pool)
{
    uint32_t *slot;

    do_raw_spin_lock_1(&idm_refill_lock);
    if (pool != 0) {
        slot = &((uint32_t *)(uintptr_t)rx_jbuf_ring)[idm_jumbo_refill_index];
        ++idm_jumbo_refill_index;
        if (idm_jumbo_refill_index >= uIDM_RX_JUMBO_BP_NUM)
            idm_jumbo_refill_index = 0;
    } else {
        slot = &((uint32_t *)(uintptr_t)rx_buf_ring)[idm_refill_index];
        ++idm_refill_index;
        if (idm_refill_index >= uIDM_RX_NORMAL_BP_NUM)
            idm_refill_index = 0;
    }
    __atomic_store_n((uint8_t *)&idm_refill_lock, 0, __ATOMIC_RELEASE);
    *slot = __fswab32_1(old_buffer);
}

void *idm_alloc_buf(uint32_t pool)
{
    uint32_t context_bits;
    struct idm_fifo *fifo;
    void *object;
    void *cache;

    context_bits = *(uint32_t *)(read_sp_el0() + 0x10U) & 0xff00U;
    if (context_bits != 0 && pool == 0U) {
        struct idm_free_stash *stash = (struct idm_free_stash *)(
            idm_free_data + __my_cpu_offset_0());

        /* Normal-pool fast cache refills from FIFO0 in bounded batches. */
        if (stash->consumer_count == 0) {
            uint32_t batch_count;

            fifo = &idm_fifo[0];
            do_raw_spin_lock_1(&fifo->lock_word);
            batch_count = fifo->in - fifo->out;
            if (batch_count == 0) {
                ++idm_fifo_empty[0];
                __atomic_store_n((uint8_t *)&fifo->lock_word, 0,
                                 __ATOMIC_RELEASE);
            } else {
                uint32_t start;
                uint32_t first_count;
                uint32_t second_count;

                if (batch_count >= 32U)
                    batch_count = 32U;
                start = fifo->out & fifo->mask;
                first_count = fifo->mask + 1U - start;
                if (first_count > batch_count)
                    first_count = batch_count;
                second_count = batch_count - first_count;
                memcpy(stash->consumer_entries, &fifo->buffer[start],
                       (size_t)first_count *
                       sizeof(stash->consumer_entries[0]));
                if (second_count != 0) {
                    memcpy(&stash->consumer_entries[first_count], fifo->buffer,
                           (size_t)second_count *
                           sizeof(stash->consumer_entries[0]));
                }
                fifo->out += batch_count;
                __atomic_store_n((uint8_t *)&fifo->lock_word, 0,
                                 __ATOMIC_RELEASE);
                ++idm_fifo_out_ncnt;
            }
            stash->consumer_count = batch_count;
        }

        if (stash->consumer_count != 0) {
            object = stash->consumer_entries[--stash->consumer_count];
            if (object != 0)
                return object;
        }
    } else {
        fifo = &idm_fifo[pool];
        do_raw_spin_lock_1(&fifo->lock_word);
        if (fifo->in == fifo->out) {
            ++idm_fifo_empty[pool];
            __atomic_store_n((uint8_t *)&fifo->lock_word, 0,
                             __ATOMIC_RELEASE);
        } else {
            object = fifo->buffer[fifo->out & fifo->mask];
            ++fifo->out;
            __atomic_store_n((uint8_t *)&fifo->lock_word, 0,
                             __ATOMIC_RELEASE);
            ++idm_fifo_out_cnt[pool];
            if (object != 0)
                return object;
        }
    }

    ++idm_debug_cnt;
    if (pool != 0) {
        cache = idm_jbuf_cache;
    } else {
        object = net_alloc_kmem();
        if (object != 0)
            return object;
        cache = idm_buf_cache;
    }

    object = kmem_cache_alloc(cache, IDM_KMALLOC_FLAGS);
    if (object == 0 && __printk_ratelimit("idm_alloc_buf"))
        printk("idm failed to alloc skb\n");
    return object;
}

void *idm_alloc_nbuf(void)
{
    uint8_t *buffer;
    uint32_t buffer_data_phys;
    uint32_t cpu;

    buffer = idm_alloc_buf(0);
    if (buffer == 0)
        return 0;

    buffer_data_phys = idm_reserved_base + 0x800U +
                       4U * (uIDM_RX_NORMAL_BP_NUM +
                             uIDM_RX_JUMBO_BP_NUM +
                             uIDM_TX_JUMBO_BP_RETRV_NUM +
                             uIDM_TX_NORMAL_BP_RETRV_NUM +
                             uIDM_TX_EXTRAL_BP_RETRV_NUM) +
                       ((uIDM_TX_QUEUE_DESC_DEPTH +
                         6U * uIDM_RX_QUEUE_DESC_DEPTH) << 7);
    cpu = *(uint32_t *)(cpu_number + __my_cpu_offset_0());
    if ((uintptr_t)buffer < IDM_LINEAR_ADDRESS(buffer_data_phys))
        ++idm_status[2U * cpu + 12U];
    else
        ++idm_status[2U * cpu + 4U];

    *(uint64_t *)(void *)(buffer + 0x00U) = 0;
    *(uint16_t *)(void *)(buffer + 0x28U) = 0;
    *(void **)(void *)(buffer + 0x10U) = buffer + 0x40U;
    *(uint32_t *)(void *)(buffer + 0x2cU) = 0;
    *(void **)(void *)(buffer + 0x18U) =
        buffer + uBP_BUFFER_OFFSET + 64U;
    return buffer;
}

int idm_fifo_in(uint32_t fifo_index, uintptr_t buffer)
{
    struct idm_fifo *fifo = &idm_fifo[fifo_index];
    uint32_t *preempt_count = (uint32_t *)(read_sp_el0() + 0x10U);
    uintptr_t return_ip = (uintptr_t)__builtin_return_address(0);

    *preempt_count += 0x200U;
    do_raw_spin_lock_1(&fifo->lock_word);
    if (fifo->mask + fifo->out - fifo->in == 0xffffffffU) {
        __atomic_store_n((uint8_t *)&fifo->lock_word, 0, __ATOMIC_RELEASE);
        __local_bh_enable_ip(return_ip, 0x200U);
        if (__printk_ratelimit("idm_fifo_in")) {
            printk("idm_fifo_in bug in %x out %x mask %x\n", fifo->in,
                   fifo->out, fifo->mask);
        }
        return -1;
    }

    fifo->buffer[fifo->in & fifo->mask] = (void *)buffer;
    ++fifo->in;
    __atomic_store_n((uint8_t *)&fifo->lock_word, 0, __ATOMIC_RELEASE);
    __local_bh_enable_ip(return_ip, 0x200U);
    ++idm_fifo_in_cnt[fifo_index];
    return 0;
}

void idm_free_buf(void *buffer, uint32_t pool)
{
    uint32_t buffer_data_phys;
    uint32_t cpu;
    uintptr_t data_boundary;

    buffer_data_phys = idm_reserved_base + 0x800U +
                       4U * (uIDM_RX_NORMAL_BP_NUM +
                             uIDM_RX_JUMBO_BP_NUM +
                             uIDM_TX_JUMBO_BP_RETRV_NUM +
                             uIDM_TX_NORMAL_BP_RETRV_NUM +
                             uIDM_TX_EXTRAL_BP_RETRV_NUM) +
                       ((uIDM_TX_QUEUE_DESC_DEPTH +
                         6U * uIDM_RX_QUEUE_DESC_DEPTH) << 7);
    data_boundary = IDM_LINEAR_ADDRESS(buffer_data_phys);
    cpu = *(uint32_t *)(cpu_number + __my_cpu_offset_0());

    if ((uintptr_t)buffer < data_boundary) {
        ++idm_status[2U * cpu + pool + 8U];
        if (pool != 0)
            kmem_cache_free(idm_jbuf_cache, buffer);
        else
            net_free_kmem(buffer);
        return;
    }

    ++idm_status[2U * cpu + pool];
    if (pool != 0 ||
        (*(uint32_t *)(read_sp_el0() + 0x10U) & 0xff00U) == 0) {
        idm_fifo_in(pool, (uintptr_t)buffer);
        return;
    }

    {
        struct idm_free_stash *stash = (struct idm_free_stash *)(
            idm_free_data + __my_cpu_offset_0());

        stash->producer_entries[stash->producer_count++] = buffer;
        if (stash->producer_count > 31U) {
            struct idm_fifo *fifo = &idm_fifo[0];
            uint32_t free_slots;

            do_raw_spin_lock_1(&fifo->lock_word);
            free_slots = fifo->mask + fifo->out - fifo->in + 1U;
            if (free_slots > 31U) {
                uint32_t start = fifo->in & fifo->mask;
                uint32_t first_count = fifo->mask + 1U - start;
                uint32_t second_count;

                if (first_count > 32U)
                    first_count = 32U;
                second_count = 32U - first_count;
                memcpy(&fifo->buffer[start], stash->producer_entries,
                       (size_t)first_count *
                       sizeof(stash->producer_entries[0]));
                if (second_count != 0) {
                    memcpy(fifo->buffer,
                           &stash->producer_entries[first_count],
                           (size_t)second_count *
                           sizeof(stash->producer_entries[0]));
                }
                fifo->in += 32U;
                __atomic_store_n((uint8_t *)&fifo->lock_word, 0,
                                 __ATOMIC_RELEASE);
                ++idm_fifo_in_ncnt;
            } else {
                __atomic_store_n((uint8_t *)&fifo->lock_word, 0,
                                 __ATOMIC_RELEASE);
                if (__printk_ratelimit("idm_fifo_in_n")) {
                    printk("idm_fifo_in_n bug in %x out %x mask %x\n", fifo->in,
                           fifo->out, fifo->mask);
                }
            }
            stash->producer_count = 0;
        }
    }
}

void idm_free_skb_data(zte_skb_t *skb)
{
    uint32_t flags = IDM_SKB_U32(skb, 0x114);
    uint32_t buffer_data_phys;
    uintptr_t data_boundary;
    void *head = IDM_SKB_PTR(skb, 0x128);

    if ((flags & 1U) == 0) {
        kfree(head);
        return;
    }

    buffer_data_phys = idm_reserved_base + 0x800U +
                       4U * (uIDM_RX_NORMAL_BP_NUM +
                             uIDM_RX_JUMBO_BP_NUM +
                             uIDM_TX_JUMBO_BP_RETRV_NUM +
                             uIDM_TX_NORMAL_BP_RETRV_NUM +
                             uIDM_TX_EXTRAL_BP_RETRV_NUM) +
                       ((uIDM_TX_QUEUE_DESC_DEPTH +
                         6U * uIDM_RX_QUEUE_DESC_DEPTH) << 7);
    data_boundary = IDM_LINEAR_ADDRESS(buffer_data_phys);

    if ((flags & 0x8000U) != 0) {
        uint32_t raw_data = IDM_SKB_U32(skb, 0x118);
        uintptr_t entry_offset =
            (uintptr_t)(uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE) - 68U -
            (uintptr_t)(uBP_BUFFER_OFFSET + 64U);
        uint8_t *entry_words = (uint8_t *)(uintptr_t)(
            ((uint64_t)raw_data - memstart_addr) |
            0xffffff8000000000ULL);
        uint8_t *entry_count_record = (uint8_t *)head +
                                      IDM_SKB_U32(skb, 0x120);
        uint32_t i;

        ++dword_288A0;
        for (i = 0; i < entry_count_record[2]; ++i) {
            uint32_t raw_buffer = *(uint32_t *)(void *)(
                entry_words + entry_offset + 4U * i);
            uint8_t *entry = (uint8_t *)(uintptr_t)(
                (((uint64_t)raw_buffer - memstart_addr) |
                 0xffffff8000000000ULL) -
                (uBP_BUFFER_OFFSET + 64U));

            if ((uintptr_t)entry < data_boundary) {
                kmem_cache_free(idm_buf_cache, entry);
            } else {
                uint32_t cpu = *(uint32_t *)(cpu_number +
                                             __my_cpu_offset_0());

                ++idm_status[2U * cpu];
                idm_fifo_in(0, (uintptr_t)entry);
            }
        }
    }

    {
        uint32_t pool = (flags >> 1) & 1U;
        uint32_t cpu = *(uint32_t *)(cpu_number + __my_cpu_offset_0());

        if ((uintptr_t)head < data_boundary) {
            ++idm_status[2U * cpu + pool + 8U];
            kmem_cache_free(pool != 0 ? idm_jbuf_cache : idm_buf_cache, head);
        } else {
            ++idm_status[2U * cpu + pool];
            idm_fifo_in(pool, (uintptr_t)head);
        }
    }
}

void dump_tx_desc(const void *descriptor)
{
    const uint8_t *bytes = descriptor;
    const uint32_t *words = descriptor;

    printk("%p,0x%.8x 0x%.8x 0x%.8x,p %d l %d,out %d,gemport %d,qid %u, idm_flag %u\n",
           descriptor, words[0], words[1], words[2], bytes[7] & 0x3fU,
           (*(const uint16_t *)(bytes + 4U) >> 1) & 0x3fffU,
           bytes[10] & 0x3fU, *(const uint16_t *)(bytes + 24U),
           *(const uint16_t *)(bytes + 26U) & 0x1ffU,
           (bytes[27] >> 2) & 3U);
    printk("soft define %.8x 0x%.8x 0x%.8x %.8x\n", words[3], words[4],
           words[5], words[6]);
}

void dump_tx_desc_wifi(const void *descriptor)
{
    const uint8_t *bytes = descriptor;
    const uint32_t *words = descriptor;

    printk("%p,0x%.8x 0x%.8x 0x%.8x,p %d l %d\n", descriptor, words[0],
           words[1], words[2], bytes[7] & 0x3fU,
           (*(const uint16_t *)(bytes + 4U) >> 1) & 0x3fffU);
    printk("soft define %.8x 0x%.8x 0x%.8x %.8x\n", words[3], words[4],
           words[5], words[6]);
}

uint32_t idm_check_all_tx_desc(uint32_t queue_index)
{
    uint8_t *descriptor;
    uint32_t index;

    if (queue_index > 3U)
        return queue_index;

    descriptor = idm_tx_q[queue_index].descriptor_base;
    for (index = 0;; ++index) {
        uint32_t depth = uIDM_TX_QUEUE_DESC_DEPTH;

        if (index >= depth)
            return depth;

        if (((IDM_TXD_U16(descriptor, 4U) >> 1) & 0x3fffU) <= 15U) {
            if (__printk_ratelimit("idm_check_all_tx_desc")) {
                printk("%d desc %p, len error \n", index, descriptor);
            }
            dump_tx_desc(descriptor);
        }
    }
}

void idm_exit(void)
{
}

static void _check_abuf(uint8_t pool_selector)
{
    uint32_t pool = pool_selector & 1U;
    uintptr_t buffer_base;
    uint32_t *refill_ring;
    uint32_t buffer_stride;
    uint32_t rx_buffer_count;
    uint32_t total_buffer_count;
    uint32_t bitmap_word_count;
    uint32_t ring_depth;
    volatile unsigned long *seen_bits;
    struct idm_fifo *fifo;
    uint32_t *preempt_count;
    uintptr_t return_ip = (uintptr_t)__builtin_return_address(0);
    uint32_t index;
    uint32_t percpu_count = 0;
    uint32_t fifo_alloc_count = 0;
    uint32_t fifo_free_count = 0;
    uint32_t kmem_alloc_count = 0;
    uint32_t kmem_free_count = 0;
    uint32_t seen_count = 0;
    uint32_t cpu;

    if (pool != 0) {
        buffer_base = idm_jumbo_buf_base;
        refill_ring = (uint32_t *)(uintptr_t)rx_jbuf_ring;
        buffer_stride = uJUMBO_BP_SIZE + 0x140U - uSKB_SHAREDINFO_SIZE;
        rx_buffer_count = uIDM_RX_JUMBO_BUFFER_NUM;
        total_buffer_count = rx_buffer_count + uIDM_TX_JUMBO_BP_NUM;
        bitmap_word_count = total_buffer_count >> 6;
        ring_depth = uIDM_RX_JUMBO_BP_NUM;
    } else {
        buffer_base = idm_normal_buf_base;
        refill_ring = (uint32_t *)(uintptr_t)rx_buf_ring;
        buffer_stride = uNORMAL_BP_SIZE + 0x140U - uSKB_SHAREDINFO_SIZE;
        rx_buffer_count = uIDM_RX_NORMAL_BUFFER_NUM;
        total_buffer_count = rx_buffer_count + uIDM_TX_NORMAL_BP_NUM;
        bitmap_word_count = total_buffer_count >> 6;
        ring_depth = uIDM_RX_NORMAL_BP_NUM;
    }

    /* The machine code allocates this diagnostic bitmap without clearing it. */
    seen_bits = __kmalloc(8U * ((bitmap_word_count + 1U) & 0x07ffffffU),
                          3520U);
    if (seen_bits == 0)
        return;

    fifo = &idm_fifo[pool];
    preempt_count = (uint32_t *)(read_sp_el0() + 0x10U);
    *preempt_count += 0x200U;
    do_raw_spin_lock_1(&fifo->lock_word);
    printk("buf in fifo %u @ %p\n", fifo->in - fifo->out, fifo);
    for (index = fifo->out; index < fifo->in; ++index) {
        void *buffer = fifo->buffer[index & fifo->mask];
        int64_t buffer_index =
            (int64_t)((uintptr_t)buffer - buffer_base) / (int64_t)buffer_stride;

        if (total_buffer_count > (uint32_t)buffer_index) {
            if (test_and_set_bit((uint32_t)buffer_index, seen_bits) != 0)
                printk("buf %p @ %u repeat\n", buffer, index & fifo->mask);
        } else {
            printk("invalid buf %p, bp %u\n", buffer, (uint32_t)buffer_index);
        }
    }
    __atomic_store_n((uint8_t *)&fifo->lock_word, 0, __ATOMIC_RELEASE);
    __local_bh_enable_ip(return_ip, 0x200U);

    cpu = UINT32_MAX;
    while ((cpu = cpumask_next(cpu, __cpu_possible_mask)) < nr_cpu_ids) {
        struct idm_free_stash *stash = (struct idm_free_stash *)(
            idm_free_data + __per_cpu_offset[cpu]);
        uint32_t producer_count = stash->producer_count;

        fifo_alloc_count += idm_status[pool + 2U * cpu + 4U];
        fifo_free_count += idm_status[pool + 2U * cpu];
        for (index = 0; index < stash->producer_count; ++index) {
            void *buffer = stash->producer_entries[index];
            int64_t buffer_index =
                (int64_t)((uintptr_t)buffer - buffer_base) /
                (int64_t)buffer_stride;

            if (total_buffer_count > (uint32_t)buffer_index) {
                if (test_and_set_bit((uint32_t)buffer_index, seen_bits) != 0)
                    printk("buf %p repeat\n", buffer);
            } else {
                printk("invalid buf %p, bp %u\n", buffer,
                       (uint32_t)buffer_index);
            }
        }

        percpu_count += producer_count + stash->consumer_count;
        for (index = 0; index < stash->consumer_count; ++index) {
            void *buffer = stash->consumer_entries[index];
            int64_t buffer_index =
                (int64_t)((uintptr_t)buffer - buffer_base) /
                (int64_t)buffer_stride;

            if (total_buffer_count > (uint32_t)buffer_index) {
                if (test_and_set_bit((uint32_t)buffer_index, seen_bits) != 0)
                    printk("buf %p repeat\n", buffer);
            } else {
                printk("invalid buf %p, bp %u\n", buffer,
                       (uint32_t)buffer_index);
            }
        }
    }

    printk("buf in percpu %u\n", percpu_count);
    printk("buf in ring %u\n", rx_buffer_count);
    {
        uint32_t ring_index = pool != 0 ? idm_jumbo_refill_index :
                                          idm_refill_index;

        for (index = 0; index != rx_buffer_count; ++index) {
            uintptr_t buffer;
            int64_t buffer_index;

            if (ring_index != 0)
                --ring_index;
            else
                ring_index = ring_depth - 1U;
            buffer = IDM_LINEAR_ADDRESS(__fswab32_1(refill_ring[ring_index]));
            buffer_index =
                (int64_t)(buffer - buffer_base) / (int64_t)buffer_stride;
            if (total_buffer_count > (uint32_t)buffer_index) {
                if (test_and_set_bit((uint32_t)buffer_index, seen_bits) != 0)
                    printk("buf %p repeat\n", (void *)buffer);
            } else {
                printk("invalid buf %p, bp %u\n", (void *)buffer,
                       (uint32_t)buffer_index);
            }
        }
    }

    for (index = 0; index != bitmap_word_count; ++index) {
        uint32_t bit;

        if (seen_bits[index] == 0x0fffffffUL) {
            seen_count += 64U;
            continue;
        }

        for (bit = 0; bit != 64U; ++bit) {
            /* Preserve the 32-bit shift plus sign extension in the ARM64 body. */
            unsigned long mask = (unsigned long)(int64_t)(int32_t)(
                1U << (bit & 31U));

            if ((seen_bits[index] & mask) != 0) {
                ++seen_count;
            } else {
                uint32_t missing_index = (index << 6) + bit;

                if (pool != 0) {
                    printk("\0014Jumbo buf not support yet!\n");
                } else {
                    uint32_t payload_offset =
                        buffer_stride * missing_index + uBP_BUFFER_OFFSET + 64U;
                    uint8_t *payload = (uint8_t *)(uintptr_t)(
                        idm_normal_buf_base + payload_offset);
                    uint32_t byte_index;

                    printk("busy buf:\n");
                    for (byte_index = 0; byte_index != 128U; ++byte_index) {
                        printk("\001c%02x ", payload[byte_index]);
                        if (byte_index != 0U && byte_index % 12U == 11U)
                            printk("\n");
                    }
                    printk("\nbusy buf end:\n");
                }
                printk("bp %u miss\n", missing_index);
            }
        }
    }

    printk("%s bp total %u miss %d\n", pool != 0 ? "jumbo" : "normal",
           seen_count, (int32_t)((bitmap_word_count << 6) - seen_count));
    printk("alloc %u, free %u,differ %d\n", fifo_alloc_count,
           fifo_free_count, (int32_t)(fifo_alloc_count - fifo_free_count));

    cpu = UINT32_MAX;
    while ((cpu = cpumask_next(cpu, __cpu_possible_mask)) < nr_cpu_ids) {
        kmem_alloc_count += idm_status[pool + 2U * cpu + 12U];
        kmem_free_count += idm_status[pool + 2U * cpu + 8U];
    }
    printk("kmem alloc %u, free %u,differ %d\n", kmem_alloc_count,
           kmem_free_count, (int32_t)(kmem_alloc_count - kmem_free_count));
    kfree((void *)seen_bits);
}

void idm_check_bppe(uint8_t pool_selector)
{
    _check_abuf(pool_selector);
}

void check_bppe(void)
{
    _check_abuf(0);
}

void set_idm_int_cpu_rx_cpu_config(uint32_t target_cpu)
{
    if (nr_cpu_ids != 2U)
        return;

    if (target_cpu == 1U) {
        irq_set_affinity_hint((unsigned int)g_idm_irq[0],
                              IDM_AFFINITY_HINT(1));
        g_idm_irq_to_cpu = target_cpu;
        eth_xmit_mode = target_cpu;
    } else {
        irq_set_affinity_hint((unsigned int)g_idm_irq[0],
                              IDM_AFFINITY_HINT(0));
        g_idm_irq_to_cpu = 0;
        eth_xmit_mode = 0;
    }
}

void do_raw_spin_lock_flags_idm_lock_int(void)
{
    __builtin_prefetch((const void *)&idm_lock_int, 1, 1);

    /* Model the LDAXR/STXR retry before handing a contended word to qspinlock. */
    for (;;) {
        uint32_t observed = 0;

        if (__atomic_compare_exchange_n(&idm_lock_int, &observed, 1U, 0,
                                        __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED)) {
            return;
        }
        if (observed != 0) {
            queued_spin_lock_slowpath(&idm_lock_int, observed, 0, 1);
            return;
        }
    }
}
