/*
 * Semantic reconstruction of CPU-net initialization at 0x0e220.
 *
 * Generic netdev/NAPI/timer declarations provide only ABI vocabulary. The
 * values, call order, names, and failure behavior are from the vendor module.
 */

#include <stdint.h>

typedef struct zte_net_device zte_net_device_t;
typedef struct zte_skb zte_skb_t;
typedef struct zte_proc_dir zte_proc_dir_t;
typedef int (*zte_smb_test_config_t)(int value);
typedef void (*zte_upload_hook_t)(int value);

typedef struct {
    uint32_t queue;
    uint32_t descriptor_byte_6_low_6;
    uint32_t descriptor_bits_7_to_12;
    uint32_t descriptor_byte_7_bit_5;
    uint32_t queue_is_15;
    uint8_t descriptor_bytes_8_to_23[16];
} zte_wifi_trap_info_t;

typedef void (*zte_idm_skb_recv_t)(zte_wifi_trap_info_t *trap_info,
                                   zte_skb_t *skb);

typedef struct zte_gro_hlist_node {
    struct zte_gro_hlist_node *next;
    struct zte_gro_hlist_node **pprev;
} zte_gro_hlist_node_t;

typedef struct zte_gro_port_node {
    struct zte_gro_port_node *next;
    struct zte_gro_port_node *prev;
} zte_gro_port_node_t;

typedef struct {
    uint16_t port;
    uint8_t opaque_2[6];
    zte_gro_port_node_t node;
} zte_gro_port_entry_t;

typedef struct {
    zte_gro_port_node_t head;
} zte_gro_port_list_t;

typedef struct {
    zte_skb_t *skb;
    uint32_t hash;
    uint32_t opaque_c;
    zte_gro_hlist_node_t node;
} zte_gro_flow_t;

typedef struct {
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
} zte_netdev_stats_t;

typedef struct zte_napi {
    uint8_t opaque_0[0x18c];
    uint32_t irq_count;
    uint32_t irq_err_count;
    uint32_t poll_count;
    uint32_t rx_int_count;
    uint32_t tx_int_count;
} zte_napi_t;

typedef struct {
    uint8_t opaque_0[0x88];
    volatile unsigned long trans_start;
} zte_netdev_tx_queue_t;

typedef struct zte_tx_descriptor {
    uint32_t data_physical;
    uint32_t word_4;
    uint32_t word_8;
    uint8_t opaque_c[12];
    uint32_t word_18;
    uint8_t opaque_1c[4];
} zte_tx_descriptor_t;

typedef struct zte_nbuf {
    struct zte_nbuf *next;
    zte_net_device_t *device;
    void *buffer_with_headroom;
    uint8_t *data;
    uint8_t opaque_20[8];
    uint16_t length;
    uint8_t opaque_2a[2];
    uint8_t flags;
    uint8_t opaque_2d[3];
    uint32_t descriptor_word_4;
    uint32_t descriptor_word_8;
    uint32_t descriptor_word_18;
} zte_nbuf_t;

typedef struct {
    zte_tx_descriptor_t *descriptor_base;
    uintptr_t *owners;
    uint32_t producer;
    uint32_t pending;
    uint32_t consumer;
    uint32_t hardware_queue;
    uint32_t depth;
    uint32_t hardware_done;
} zte_tx_queue_t;

typedef struct {
    uint8_t *descriptor_base;
    uint32_t producer;
    uint32_t depth;
} zte_rx_queue_t;

typedef struct {
    uint64_t mask;
    uint64_t value;
    uint32_t shift;
} zte_net_dump_condition_t;

typedef struct {
    uint8_t opaque_0[16];
    uint64_t expires;
    uint8_t opaque_18[16];
} zte_timer_t;

typedef int (*zte_napi_poll_t)(zte_napi_t *napi, int budget);
typedef void (*zte_timer_fn_t)(zte_timer_t *timer);
typedef void (*zte_low_power_send_t)(unsigned int arg0, unsigned int arg1,
                                     const void *data, unsigned int length,
                                     unsigned int arg4);
typedef int (*zte_low_power_up_en_judge_t)(void);
typedef void (*zte_omci_oam_rx_t)(const void *data, unsigned int length,
                                   unsigned int port);
typedef int (*zte_omci_mic_handler_t)(const void *data, unsigned int length);
typedef struct {
    uintptr_t completion_ring_base;
    uint32_t ring_size;
    uint32_t release_index;
    uint32_t release_count;
} zte_recycle_context_t;
typedef void (*zte_recycle_callback_t)(unsigned int queue,
                                       zte_recycle_context_t *context);

struct cpu_net_ops_prefix {
    void (*mask_source)(uint32_t bits);
    void (*unmask_source)(uint32_t bits);
    uint32_t (*get_rx_count)(unsigned int queue);
    void *(*get_rx_queue)(unsigned int index);
    void *(*get_tx_queue)(unsigned int index);
    void *(*alloc_buffer)(void);
    void (*free_buffer)(void *buffer, unsigned int pool);
    void (*flush_rx_refill)(void);
    int (*refill_rx_buffer)(uint32_t old_buffer, unsigned int pool,
                            unsigned int reuse);
    void (*update_rx_queue)(unsigned int queue, unsigned int count,
                            unsigned int jumbo_count);
    uint8_t opaque_50[0x20];
    void (*update_tx_queue)(unsigned int queue, unsigned int count);
};

#define NETDEV_U32(device, offset) \
    (*(uint32_t *)((uint8_t *)(device) + (offset)))
#define NETDEV_PTR(device, offset) \
    (*(void **)((uint8_t *)(device) + (offset)))
#define NETDEV_INDIRECT_U64(device, pointer_offset, field_offset) \
    (*(volatile uint64_t *)((uint8_t *)NETDEV_PTR(device, pointer_offset) + \
                            (field_offset)))
#define RXD_U32(descriptor, offset) \
    (*(uint32_t *)((uint8_t *)(descriptor) + (offset)))
#define RXD_U16(descriptor, offset) \
    (*(uint16_t *)((uint8_t *)(descriptor) + (offset)))
#define RXD_U8(descriptor, offset) \
    (*(uint8_t *)((uint8_t *)(descriptor) + (offset)))
#define SKB_PTR(skb, offset) \
    (*(void **)((uint8_t *)(skb) + (offset)))
#define SKB_U32(skb, offset) \
    (*(uint32_t *)((uint8_t *)(skb) + (offset)))
#define SKB_U16(skb, offset) \
    (*(uint16_t *)((uint8_t *)(skb) + (offset)))
#define SKB_U8(skb, offset) \
    (*(uint8_t *)((uint8_t *)(skb) + (offset)))
#define NET_TST_TX_ALLOC_FLAGS 0xa20U

extern int printk(const char *format, ...);
extern int isCpuType_133(void);
extern int isCpuType_129(void);
zte_net_device_t *cpu_net_register(unsigned int type, const char *name);
extern zte_net_device_t *alloc_etherdev_mqs(unsigned int private_size,
                                             unsigned int queue_count);
extern int register_netdev(zte_net_device_t *device);
extern void free_netdev(zte_net_device_t *device);
extern char *strncpy(char *destination, const char *source, unsigned long size);
extern int strncmp(const char *left, const char *right, unsigned long size);
extern int strcmp(const char *left, const char *right);
extern void netif_napi_add(zte_net_device_t *device, zte_napi_t *napi,
                           zte_napi_poll_t poll, int weight);
extern void netif_carrier_on(zte_net_device_t *device);
extern void netif_carrier_off(zte_net_device_t *device);
extern void netif_tx_wake_queue(zte_netdev_tx_queue_t *queue);
extern void napi_enable(zte_napi_t *napi);
extern void napi_disable(zte_napi_t *napi);
extern int napi_schedule_prep(zte_napi_t *napi);
extern void __napi_schedule(zte_napi_t *napi);
extern int napi_complete_done(zte_napi_t *napi, int work_done);
extern void testftp_init(void);
extern void net_gro_init(void);
extern int lower_net_smb_test_config(int value);
extern int irq_set_affinity_hint(unsigned int irq, const void *mask);
extern zte_proc_dir_t *proc_mkdir(const char *name, zte_proc_dir_t *parent);
extern void *proc_create(const char *name, unsigned int mode,
                         zte_proc_dir_t *parent, const void *operations);
extern int __printk_ratelimit(const char *function_name);
extern void net_upload_fun(int value);
extern void gso_upload_enable(void);
extern void gso_upload_disable(unsigned int release_buffers);
extern void *cpu_net_alloc_nbuf(void);
extern void cpu_net_free_nbuf(void *nbuf);
int dump_net_check(const void *data, unsigned int length);
extern void dump_tx_desc(const void *descriptor);
void dump_net_data(const void *data, unsigned int length);
extern void *memset(void *destination, int value, unsigned long size);
extern int upload_user_range_ok(const void *user_pointer, unsigned long size);
extern unsigned long __arch_copy_from_user(void *to, const void *from,
                                           unsigned long size);
extern unsigned long simple_strtoul(const char *text, char **end_pointer,
                                     unsigned int base);
extern void *net_get_next_txdesc(void *queue);
extern uint32_t net_check_tx_done_nolock(zte_tx_queue_t *queue);
extern void net_cfg_desc_by_skb(void *descriptor, zte_skb_t *skb,
                                unsigned int direction);
extern uint64_t virt_to_phys(const void *pointer);
extern int cpu_net_nb_desc_tx(void *nbuf, void *descriptor);
extern void dsb_st(void);
extern uint8_t *network_hdr_optimized(void);
extern void net_gso_checksum_upload(void *ipv4_header, void *tcp_header,
                                    unsigned int upload_mode);
extern void net_gso_ipv6tcp_checksum_constprop_6(void *ipv6_header,
                                                  void *tcp_header);
extern void *memcpy(void *destination, const void *source, unsigned long size);
extern uint32_t csum_partial(const void *buffer, unsigned int length,
                             uint32_t initial_sum);
extern uint32_t csum_tcpudp_nofold(uint32_t source, uint32_t destination,
                                   unsigned int length, unsigned int protocol,
                                   uint32_t sum);
extern uint16_t csum_ipv6_magic(const void *source, const void *destination,
                                unsigned int length, unsigned int protocol,
                                uint32_t sum);
extern uint64_t read_icc_pmr_el1(void);
extern void write_icc_pmr_el1(uint64_t value);
extern void dsb_sy(void);
extern uintptr_t read_tpidr_el1(void);
extern uint64_t read_tpidr_el2(void);
extern void net_gso_init(void);
extern void init_timer_key(zte_timer_t *timer, zte_timer_fn_t function,
                           unsigned int flags, const char *name, void *key);
extern int add_timer_on(zte_timer_t *timer, int cpu);
extern void idm_recycle_init(void);
extern void pp_tcp_gro_flush_all(void);
extern void pp_tcp_gro_flush(zte_skb_t *skb);
extern void *kmem_cache_alloc(void *cache, unsigned int flags);
extern void kfree(void *pointer);
extern int is_l4port_supported(uint16_t port, unsigned int destination);
extern void __raw_spin_lock_bh_constprop_13(void);
extern void __local_bh_enable_ip(uintptr_t ip, unsigned int offset);
extern void ip_send_check(void *ipv4_header);
extern uint64_t read_sp_el0(void);
extern void queued_spin_lock_slowpath(uint32_t *lock, uint32_t observed,
                                      uintptr_t argument_2,
                                      unsigned int argument_3);
extern int can_tcp_gro(zte_skb_t *flow_skb, const uint8_t *ipv4,
                       const uint8_t *tcp, const uint8_t *descriptor);
extern void do_raw_spin_lock(volatile uint32_t *lock);
extern void net_check_reorder_rls_nolock(void);
uint8_t *get_next_rxdesc(zte_rx_queue_t *queue);
zte_netdev_stats_t *cpu_dev_stat(zte_net_device_t *device);
extern int cpu_omci_rx(void *descriptor, void *metadata, const void *data,
                       unsigned int length);
extern int testftp_net_report(const void *data, const uint8_t *metadata,
                              unsigned int length);
extern int pp_net_tcp_gro(void *descriptor, zte_net_device_t *device,
                          const void *data, unsigned int queue,
                          unsigned int jumbo);
extern zte_skb_t *alloc_skb_attach_buffer(void *head, void *buffer,
                                          unsigned int capacity,
                                          unsigned int data_offset,
                                          unsigned int raw_buffer);
extern void *net_alloc_skb(void);
extern zte_skb_t *__netdev_alloc_skb(zte_net_device_t *device,
                                     unsigned int size,
                                     unsigned int allocation_flags);
extern void *__kmalloc(unsigned int size, unsigned int allocation_flags);
extern void skb_put(zte_skb_t *skb, unsigned int length);
extern int cpu_net_tx(zte_skb_t *skb, zte_net_device_t *device);
extern int cpu_sw_rx(zte_skb_t *skb, zte_net_device_t *device,
                     void *descriptor, void *metadata, const void *data,
                     unsigned int queue);
extern uint16_t eth_type_trans(zte_skb_t *skb, zte_net_device_t *device);
extern void netif_receive_skb(zte_skb_t *skb);
extern int cpu_net_poll(zte_napi_t *napi, int budget);
extern int cpu_idm_poll(zte_napi_t *napi, int budget);
extern int cpu_rls_poll(zte_napi_t *napi, int budget);
extern int idm_net_poll(zte_napi_t *napi, int budget);
extern int cpu_net_rx(unsigned int count, unsigned int queue,
                      unsigned int jumbo);
extern int idm_net_rx(unsigned int count, unsigned int jumbo);
extern void cpu_timer_func(zte_timer_t *timer);
extern void cpu_timer_unlock(zte_timer_t *timer);

extern struct cpu_net_ops_prefix *cpu_net_ops;
extern uint32_t uIDM_TX_NORMAL_BP_RETRV_NUM;
extern uint32_t uIDM_TX_JUMBO_BP_RETRV_NUM;
extern uint32_t uIDM_TX_EXTRAL_BP_RETRV_NUM;
extern uint32_t uIDM_RX_NORMAL_BP_NUM;
extern uint32_t uIDM_RX_JUMBO_BP_NUM;
extern uint32_t uIDM_TX_QUEUE_DESC_DEPTH;
extern uint32_t uIDM_RX_QUEUE_DESC_DEPTH;
extern uint32_t uNPPT_IDM_DESC_MODE;
extern uint32_t idm_reserved_base;
extern uint32_t g_pon_work_mode;
extern uint32_t rls_ring_size[3];
extern uint32_t rls_ring_num_max;
extern uint32_t cpu2unlock_tq[];
extern uint32_t cpu_unlock_state_1;
extern uint32_t cpu_unlock_state_2;
extern uint32_t cpu_unlock_state_3;
extern int ipsec_tx_cpu;
extern unsigned long jiffies;

extern void *omcioam_tq;
extern void *cpu_tq;
extern void *idm_tq;
extern zte_tx_queue_t *unlock_tq[2];
extern uint8_t cpu_number[];
extern zte_net_device_t *cpu_netdev_slots[4];
#define cpu_netdev (cpu_netdev_slots[0])
#define sw_netdev (cpu_netdev_slots[1])
#define omcioam_netdev (cpu_netdev_slots[2])
#define idm_netdev_object (cpu_netdev_slots[3])
extern zte_net_device_t **idm_netdev_slot;
extern zte_napi_t int_info[4];
#define IDM_NET_NAPI (&int_info[1])
#define CPU_IDM_NAPI (&int_info[2])
#define CPU_RLS_NAPI (&int_info[3])
extern zte_timer_t cpu_net_timer;
extern zte_timer_t cpu_unlock_timer[2];
extern uint32_t omcioam_lock_tx;
extern uint32_t net_lock_tx;
extern uint32_t idm_lock_tx;
extern uint32_t cpu_net_info_word_0;
extern uint32_t cpu_net_info_word_4;
extern uint32_t cpu_net_info_word_8;
extern uint32_t cpu_net_info_word_c;
extern uint32_t cpu_net_poll_calls;
extern uint32_t cpu_idm_poll_calls;
extern uint32_t idm_net_poll_calls;
extern uint32_t cpu_rls_poll_calls;
extern void (*idm_recv_cmpl)(void);
extern uint64_t memstart_addr;
extern uint64_t kimage_voffset;
extern uint64_t vabits_actual;
extern uint32_t uBP_BUFFER_OFFSET;
extern uint32_t uNORMAL_BP_SIZE;
extern uint32_t uJUMBO_BP_SIZE;
extern uint32_t uSKB_SHAREDINFO_SIZE;
extern uint32_t net_rx_cnt[64];
extern uint32_t net_gro_en;
extern uint32_t net_gro_packet_count;
extern zte_gro_hlist_node_t *gro_hash_table[16];
extern void *gro_state_cache;
extern uint32_t g_cur_flows;
extern uint32_t max_gro;
extern uint32_t net_gro_debug;
extern uint32_t net_gro_bad_length;
extern uint32_t net_gro_unsupported_port;
extern uint32_t net_gro_alloc_failure;
extern uint32_t net_gro_new_flow_count;
extern uint32_t net_gro_multi_flow_count;
extern uint32_t wifi_gro_rxq;
extern uint32_t wifi_gro_desc[8];
extern zte_net_device_t *wifi_gro_netdev;
extern uint32_t net_gro_cnt;
extern uint32_t net_gro_segment_count;
extern uint32_t net_smb_state;
extern uint32_t net_gro_flush_count;
extern zte_smb_test_config_t pp_smb_test_config;
extern uint32_t g_idm_irq[4];
extern uint32_t g_idm_irq_to_cpu;
extern unsigned long cpu_bit_bitmap[];
extern uint16_t g_net_check_threshold;
extern uint32_t net_tx_full;
extern int32_t net_tx_debug;
extern uint8_t g_net_dump_select;
extern zte_net_dump_condition_t g_net_dump_condition[2];
extern const char add_supported_l4port_alloc_failure[];
extern uint32_t g_upload_driver_en;
extern const uint8_t upload_test_fops[];
extern zte_upload_hook_t upload_hook;
extern int32_t net_gso_debug;
extern int32_t upload_count;
extern const uint8_t net_lock_slowpath_context[];
extern uint32_t gso_buf_cnt;
extern uint32_t gso_buf_idx;
extern void *gso_nbuf_pool[64];
extern uint32_t s_gso_last_hlen[64];
extern uint32_t g_nb_not_rls_cnt;
extern uint32_t gso_upload_desc_unavailable;
extern uint32_t gso_upload_send_count;
extern uint32_t gso_upload_attempt_count;
extern uint32_t gso_upload1_attempt_count;
extern uint32_t gso_upload_nbuf_unavailable;
extern uint32_t gso_upload_ipv4_count;
extern uint32_t gso_upload_ipv6_count;
extern uint32_t gso_normal_attempt_count;
extern uint32_t gso_normal_success_count;
extern uint32_t net_hw_checksum;
extern uint32_t lan_up;
extern zte_gro_port_list_t supported_source_ports;
extern zte_gro_port_list_t supported_dest_ports;
extern uint32_t groport_busy_lock;
extern void (*switch_skb_recv)(zte_skb_t *skb);
extern uint32_t idm_recv_jumbo_error;
extern uint32_t cpu_sw_trap_count;
extern uint32_t np1_trap_count;
extern uint32_t descriptor_byte_7_bit_5_count;
extern uint32_t np2_rx_trap_count;
extern int32_t np1_trap_debug;
extern int32_t idm_rx_debug;
extern uint32_t testftp_cnt;
extern uint32_t testftp_unhandled_type_count;
extern int32_t testftp_debug_cnt;
extern uint32_t g_speedtesthffenable;
extern int ffe_pre_process_zte(unsigned int task, unsigned int payload_length,
                               unsigned int received_length,
                               const void *transport_header);
extern void write_pan(unsigned int enable);
uint8_t idm_set_wifi_trap_info(const void *descriptor,
                               zte_wifi_trap_info_t *output,
                               unsigned int queue);
extern zte_idm_skb_recv_t idm_skb_recv;
extern zte_omci_oam_rx_t omci_oam_rx;
extern zte_omci_mic_handler_t omci_mic_add;
extern zte_omci_mic_handler_t omci_mic_check;
extern zte_recycle_callback_t idm_recycle_cb[3];
extern void *idm_wlanname_to_essid;
extern uint32_t local_omci_port_id;
extern void *cpu_net_netdev_ops;
extern void *idm_net_netdev_ops;
extern uint8_t default_mac[6];
extern uint8_t cpu_net_last_type;
extern zte_low_power_send_t low_power_send;
extern zte_low_power_up_en_judge_t low_power_up_en_judge;

void hlist_del_init(zte_gro_hlist_node_t *node)
{
    zte_gro_hlist_node_t *next = node->next;
    zte_gro_hlist_node_t **pprev = node->pprev;

    if (pprev == 0)
        return;

    *pprev = next;
    if (next != 0)
        next->pprev = pprev;
    node->next = 0;
    node->pprev = 0;
}

void napi_complete(zte_napi_t *napi)
{
    napi_complete_done(napi, 0);
}

int cpu_net_init(void)
{
    const char *management_name;
    unsigned int i;

    rls_ring_size[0] = uIDM_TX_NORMAL_BP_RETRV_NUM;
    cpu2unlock_tq[0] = 0;
    cpu_unlock_state_1 = 0;
    cpu_unlock_state_2 = 0;
    cpu_unlock_state_3 = 0;
    rls_ring_size[1] = uIDM_TX_JUMBO_BP_RETRV_NUM;
    rls_ring_size[2] = uIDM_TX_EXTRAL_BP_RETRV_NUM;
    if (isCpuType_133() || isCpuType_129())
        rls_ring_num_max = 3;

    omcioam_tq = cpu_net_ops->get_tx_queue(0);
    cpu_tq = cpu_net_ops->get_tx_queue(1);
    idm_tq = cpu_net_ops->get_tx_queue(2);
    unlock_tq[0] = cpu_net_ops->get_tx_queue(3);
    unlock_tq[1] = unlock_tq[0];
    if (cpu_tq == 0 || idm_tq == 0 || unlock_tq[0] == 0)
        return -1;

    sw_netdev = cpu_net_register(1, "sw");
    if (sw_netdev == 0) {
        printk("failed to register sw net\n");
        return -1;
    }

    cpu_netdev = cpu_net_register(0, "pon");
    if (cpu_netdev == 0) {
        printk("failed to register pon net\n");
        return -1;
    }

    management_name = (g_pon_work_mode & 0xe40U) != 0 ? "omci" : "oam";
    omcioam_netdev = cpu_net_register(2, management_name);
    if (omcioam_netdev == 0) {
        printk("failed to register omci/oam net\n");
        return -1;
    }

    idm_netdev_object = cpu_net_register(3, "idm");
    if (idm_netdev_object == 0) {
        printk("failed to register idm net\n");
        return -1;
    }

    netif_napi_add(cpu_netdev, &int_info[0], cpu_net_poll, 512);
    netif_napi_add(cpu_netdev, CPU_IDM_NAPI, cpu_idm_poll, 512);
    netif_napi_add(cpu_netdev, CPU_RLS_NAPI, cpu_rls_poll, 512);
    netif_napi_add(idm_netdev_object, IDM_NET_NAPI, idm_net_poll, 512);

    testftp_init();
    net_gro_init();
    net_gso_init();

    init_timer_key(&cpu_net_timer, cpu_timer_func, 0, 0, 0);
    cpu_net_timer.expires = jiffies + 1;
    add_timer_on(&cpu_net_timer, 0);
    for (i = 0; i < 2; ++i) {
        init_timer_key(&cpu_unlock_timer[i], cpu_timer_unlock, 0, 0, 0);
        cpu_unlock_timer[i].expires = jiffies + 1;
        add_timer_on(&cpu_unlock_timer[i], ipsec_tx_cpu);
    }

    idm_recycle_init();
    idm_netdev_slot = &cpu_netdev;
    omcioam_lock_tx = 0;
    net_lock_tx = 0;
    idm_lock_tx = 0;
    printk("pp net init ok,share %d\n", 320);
    return 0;
}

uint8_t *get_next_rxdesc(zte_rx_queue_t *queue)
{
    uint32_t producer = queue->producer;
    uint32_t descriptor_offset =
        producer << ((uNPPT_IDM_DESC_MODE + 5U) & 31U);
    uint8_t *descriptor = queue->descriptor_base + descriptor_offset;
    uint32_t next_producer;
    uint32_t depth;

    __builtin_prefetch(descriptor);
    next_producer = producer + 1U;
    depth = queue->depth;
    queue->producer = next_producer;
    if (next_producer >= depth)
        queue->producer = 0;
    return descriptor;
}

zte_net_device_t *cpu_net_register(unsigned int type, const char *name)
{
    zte_net_device_t *device;
    uint8_t *mac;
    unsigned int i;

    device = alloc_etherdev_mqs(200, 1);
    if (device == 0)
        return 0;

    NETDEV_PTR(device, 0x880) = device;
    NETDEV_U32(device, 0x888) = type;
    NETDEV_PTR(device, 0x1f8) =
        type == 3 ? idm_net_netdev_ops : cpu_net_netdev_ops;
    NETDEV_U32(device, 0x460) = 500;
    strncpy((char *)device, name, 15);

    mac = NETDEV_PTR(device, 0x328);
    cpu_net_last_type = (uint8_t)type;
    for (i = 0; i < 6; ++i)
        mac[i] = default_mac[i];

    if (strncmp((char *)device, "omci", 4) == 0)
        NETDEV_U32(device, 0x234) = 2000;
    if (register_netdev(device) < 0) {
        free_netdev(device);
        return 0;
    }

    return device;
}

int cpu_net_open(zte_net_device_t *device)
{
    /* The binary clears this bit with an LDXR/STXR loop. */
    NETDEV_INDIRECT_U64(device, 0x3c0, 0x90) &= ~1ULL;
    netif_carrier_on(device);

    if (strcmp((const char *)device, "pon") == 0) {
        napi_enable(&int_info[0]);
        cpu_net_ops->unmask_source(cpu_net_info_word_0);
        napi_enable(CPU_IDM_NAPI);
        cpu_net_ops->unmask_source(cpu_net_info_word_8);
    } else if (strcmp((const char *)device, "idm") == 0) {
        napi_enable(IDM_NET_NAPI);
        cpu_net_ops->unmask_source(cpu_net_info_word_4);
        napi_enable(CPU_RLS_NAPI);
        cpu_net_ops->unmask_source(cpu_net_info_word_c);
    }

    return 0;
}

int cpu_net_stop(zte_net_device_t *device)
{
    /* The binary sets this bit with an LDXR/STXR loop. */
    NETDEV_INDIRECT_U64(device, 0x3c0, 0x90) |= 1ULL;
    netif_carrier_off(device);

    if (strcmp((const char *)device, "pon") == 0) {
        napi_disable(&int_info[0]);
        cpu_net_ops->mask_source(cpu_net_info_word_0);
        napi_disable(CPU_IDM_NAPI);
        cpu_net_ops->mask_source(cpu_net_info_word_8);
    } else if (strcmp((const char *)device, "idm") == 0) {
        napi_disable(IDM_NET_NAPI);
        cpu_net_ops->mask_source(cpu_net_info_word_4);
        napi_disable(CPU_RLS_NAPI);
        cpu_net_ops->mask_source(cpu_net_info_word_c);
    }

    return 0;
}

void cpu_net_timeout(zte_net_device_t *device)
{
    zte_netdev_tx_queue_t *queue = NETDEV_PTR(device, 0x3c0);

    netif_tx_wake_queue(queue);
    if (queue->trans_start != jiffies)
        queue->trans_start = jiffies;
}

void dump_net_condition_set(uint8_t print_type, uint8_t condition_index,
                            uint64_t mask, uint64_t value, uint32_t shift)
{
    zte_net_dump_condition_t *condition;
    const uint8_t *mask_0;
    const uint8_t *value_0;
    const uint8_t *mask_1;
    const uint8_t *value_1;

    g_net_dump_select = print_type;
    if (condition_index > 1U)
        return;

    condition = &g_net_dump_condition[condition_index];
    condition->mask = __builtin_bswap64(mask);
    condition->value = __builtin_bswap64(value);
    condition->shift = shift;

    switch (print_type) {
    case 0:
        printk("\nset print skb: all pkt\n");
        break;
    case 1:
        printk("\nset print skb: pkt meet condion0\n");
        break;
    case 2:
        printk("\nset print skb: pkt meet !condion0\n");
        break;
    case 3:
        printk("\nset print skb: pkt meet condion0||condion1\n");
        break;
    case 4:
        printk("\nset print skb: pkt meet condion0&&condion1\n");
        break;
    default:
        break;
    }

    mask_0 = (const uint8_t *)&g_net_dump_condition[0].mask;
    value_0 = (const uint8_t *)&g_net_dump_condition[0].value;
    mask_1 = (const uint8_t *)&g_net_dump_condition[1].mask;
    value_1 = (const uint8_t *)&g_net_dump_condition[1].value;
    printk("   condition0: mask:%x%x%x%x%x%x%x%x value:%x%x%x%x%x%x%x%x shift:%u\n",
           mask_0[0], mask_0[1], mask_0[2], mask_0[3], mask_0[4], mask_0[5],
           mask_0[6], mask_0[7], value_0[0], value_0[1], value_0[2], value_0[3],
           value_0[4], value_0[5], value_0[6], value_0[7],
           g_net_dump_condition[0].shift);
    /* The binary prints condition 0's shift in the condition 1 line as well. */
    printk("   condition1: mask:%x%x%x%x%x%x%x%x value:%x%x%x%x%x%x%x%x shift:%u\n",
           mask_1[0], mask_1[1], mask_1[2], mask_1[3], mask_1[4], mask_1[5],
           mask_1[6], mask_1[7], value_1[0], value_1[1], value_1[2], value_1[3],
           value_1[4], value_1[5], value_1[6], value_1[7],
           g_net_dump_condition[0].shift);
}

int dump_net_check(const void *data, unsigned int length)
{
    const uint8_t *packet = data;
    const zte_net_dump_condition_t *condition_0 = &g_net_dump_condition[0];
    const zte_net_dump_condition_t *condition_1 = &g_net_dump_condition[1];
    uint64_t word;

    switch (g_net_dump_select) {
    case 0:
        return 0;
    case 1:
        if ((uint64_t)condition_0->shift + 8U > length)
            return -1;
        word = *(const uint64_t *)(const void *)(packet + condition_0->shift);
        return (word & condition_0->mask) == condition_0->value ? 0 : -1;
    case 2:
        if ((uint64_t)condition_0->shift + 8U > length)
            return -1;
        word = *(const uint64_t *)(const void *)(packet + condition_0->shift);
        return (word & condition_0->mask) == condition_0->value ? -1 : 0;
    case 3:
        if ((uint64_t)condition_0->shift + 8U > length ||
            (uint64_t)condition_1->shift + 8U > length)
            return -1;
        word = *(const uint64_t *)(const void *)(packet + condition_0->shift);
        if ((word & condition_0->mask) == condition_0->value)
            return 0;
        word = *(const uint64_t *)(const void *)(packet + condition_1->shift);
        return (word & condition_1->mask) == condition_1->value ? 0 : -1;
    case 4:
        if ((uint64_t)condition_0->shift + 8U > length ||
            (uint64_t)condition_1->shift + 8U > length)
            return -1;
        word = *(const uint64_t *)(const void *)(packet + condition_0->shift);
        if ((word & condition_0->mask) != condition_0->value)
            return -1;
        word = *(const uint64_t *)(const void *)(packet + condition_1->shift);
        return (word & condition_1->mask) == condition_1->value ? 0 : -1;
    default:
        return 0;
    }
}

void dump_net_data(const void *data, unsigned int length)
{
    const uint8_t *bytes = data;
    unsigned int dump_length = length > 0xc0U ? 0xc0U : length;
    unsigned int i;

    for (i = 0; i < dump_length; ++i) {
        printk("\1c%.2x ", bytes[i]);
        if ((i & 0xfU) == 0xfU)
            printk("\1c\n");
    }
    printk("\n");
}

void dump_net_desc(const void *descriptor, int trap_detail_format)
{
    const uint8_t *bytes = descriptor;
    const uint32_t *words = descriptor;
    uint8_t flags;

    printk("%px,0x%.8x 0x%.8x:p %u,len %u o %d t %u\n", descriptor,
           words[0], words[1], bytes[6] & 0x3fU, words[1] & 0x3fffU,
           bytes[5] >> 7, (bytes[5] & 0x40U) != 0);
    printk("soft define 0x%.8x 0x%.8x 0x%.8x 0x%.8x\n",
           words[2], words[3], words[4], words[5]);

    if (trap_detail_format != 0) {
        printk("  trap reason %u detail %u ssid %u\n",
               bytes[8], bytes[9], bytes[10] & 0x3fU);
        return;
    }

    flags = bytes[14];
    printk("  trap reason %u l3 off %d id16 %d "
           "(l3 en:%u,ipv6:%u,ipv4:%u,udp:%u,tcp:%u)\n",
           bytes[8], bytes[13], *(const uint16_t *)(const void *)(bytes + 14),
            flags & 1U, (flags >> 1) & 1U, (flags >> 2) & 1U,
            (flags >> 3) & 1U, (flags >> 4) & 1U);
}

uint8_t idm_set_wifi_trap_info(const void *descriptor,
                               zte_wifi_trap_info_t *output,
                               unsigned int queue)
{
    const uint8_t *bytes = descriptor;
    const uint32_t *words = descriptor;
    const uint8_t reason = bytes[8];
    const unsigned int length = words[1] & 0x3fffU;
    const uint8_t *data = (const uint8_t *)(uintptr_t)(
        ((uint64_t)words[0] - memstart_addr) | 0xffffff8000000000ULL);

    output->queue = queue;
    output->descriptor_byte_6_low_6 = bytes[6] & 0x3fU;
    output->descriptor_bits_7_to_12 =
        (((uint16_t)bytes[6] | ((uint16_t)bytes[7] << 8)) >> 7) & 0x3fU;
    output->descriptor_byte_7_bit_5 = (bytes[7] >> 5) & 1U;
    output->queue_is_15 = queue == 15U;
    memcpy(output->descriptor_bytes_8_to_23, bytes + 8, 16);

    if ((uint8_t)(reason - 0x62U) <= 1U) {
        int32_t debug_budget = np1_trap_debug;

        ++np1_trap_count;
        if (debug_budget > 0 && dump_net_check(data, length) == 0) {
            np1_trap_debug = debug_budget - 1;
            printk("reason 0x%x,detail 0x%x\n", reason, bytes[9]);
            dump_net_desc(descriptor, 1);
            printk("np1 trap data: %px,len %d\n", data, length);
            dump_net_data(data, length);
        }
    } else if (reason == 0x65U) {
        int32_t debug_budget = idm_rx_debug;

        ++np2_rx_trap_count;
        if (debug_budget > 0 && dump_net_check(data, length) == 0) {
            idm_rx_debug = debug_budget - 1;
            printk("reason 0x%x,detail 0x%x\n", reason, bytes[9]);
            dump_net_desc(descriptor, 1);
            printk("np2 rx trap data: %px,len %d\n", data, length);
            dump_net_data(data, length);
        }
    }

    if ((bytes[7] & 0x20U) != 0)
        ++descriptor_byte_7_bit_5_count;

    return bytes[7];
}

zte_netdev_stats_t *cpu_dev_stat(zte_net_device_t *device)
{
    if (device == 0 || (uintptr_t)device == (uintptr_t)-0x880)
        return 0;

    return (zte_netdev_stats_t *)((uint8_t *)device + 0x890U);
}

zte_netdev_stats_t *cpu_eth_get_stats(zte_net_device_t *device)
{
    return cpu_dev_stat(device);
}

void cpu_net_free_buf(void *buffer, unsigned int pool)
{
    cpu_net_ops->free_buffer(buffer, pool);
}

void cpu_net_int(unsigned int source)
{
    zte_napi_t *napi = &int_info[source];

    ++napi->irq_count;
    if (napi_schedule_prep(napi))
        __napi_schedule(napi);
    else
        ++napi->irq_err_count;
}

void dump_net_int_info(unsigned int source)
{
    zte_napi_t *napi;

    if (source > 3U) {
        printk("invalid int %d\n", source);
        return;
    }

    printk("net int %d\n", source);
    napi = &int_info[source];
    printk("irq     %u\n", napi->irq_count);
    printk("irq_err %u\n", napi->irq_err_count);
    printk("poll    %u\n", napi->poll_count);
    printk("rx int  %u\n", napi->rx_int_count);
    printk("tx int  %u\n", napi->tx_int_count);
}

uint32_t __fswab32(uint32_t value)
{
    return ((value & 0x000000ffU) << 24) |
           ((value & 0x0000ff00U) << 8) |
           ((value & 0x00ff0000U) >> 8) |
           ((value & 0xff000000U) >> 24);
}

uint32_t sub_E5C8(uint64_t priority_mask)
{
    uint64_t readback;

    write_icc_pmr_el1(priority_mask);
    dsb_sy();
    readback = read_icc_pmr_el1();
    write_icc_pmr_el1((uint32_t)readback ^ 0xe0U);
    (void)read_tpidr_el2();
    return __fswab32((uint32_t)priority_mask);
}

uint64_t virt_to_phys(const void *virtual_address)
{
    uint64_t address = (uint64_t)(uintptr_t)virtual_address;
    uint32_t va_bits = (uint32_t)vabits_actual;
    uint64_t direct_map_limit =
        0x8000000000ULL - (1ULL << (va_bits - 1U));

    if ((address ^ 0xffffff8000000000ULL) >= direct_map_limit)
        return address - kimage_voffset;

    return (address & 0x7fffffffffULL) + memstart_addr;
}

int testftp_net_report(const void *data, const uint8_t *metadata,
                       unsigned int received_length)
{
    const uint8_t *packet = data;
    const uint8_t *transport_header;
    unsigned int network_length;
    unsigned int payload_length;
    unsigned int task;
    uint8_t flags;

    ++testftp_cnt;
    flags = metadata[6];
    if ((flags & 4U) != 0) {
        const uint8_t *network_header = packet + metadata[5];
        unsigned int network_header_length = 4U * (network_header[0] & 0x0fU);

        transport_header = network_header + network_header_length;
        network_length = ((unsigned int)network_header[2] << 8) |
                         network_header[3];
        payload_length = network_length - network_header_length -
                         4U * (transport_header[12] >> 4);
    } else {
        if ((flags & 2U) == 0) {
            ++testftp_unhandled_type_count;
            return (int)testftp_unhandled_type_count;
        }

        transport_header = packet + metadata[5] + 40U;
        network_length = ((unsigned int)packet[metadata[5] + 4U] << 8) |
                         packet[metadata[5] + 5U];
        payload_length = network_length - 4U * (transport_header[12] >> 4);
    }

    task = metadata[7];
    if (testftp_debug_cnt > 0) {
        --testftp_debug_cnt;
        printk("testftp task %u,len %u/%u/%u\n", task, payload_length,
               network_length, received_length);
    }
    return ffe_pre_process_zte(task, payload_length, received_length,
                                transport_header);
}

int sub_FA60(const void *data, const uint8_t *metadata,
             unsigned int received_length)
{
    write_pan(0U);
    write_pan(1U);
    return testftp_net_report(data, metadata, received_length);
}

void testftp_init(void)
{
    g_speedtesthffenable = 1U;
}

int cpu_net_poll(zte_napi_t *napi, int budget)
{
    int remaining = budget;
    unsigned int processed = 0;
    unsigned int scan_mask = 0xff;
    unsigned int pass;

    (void)napi;
    ++cpu_net_poll_calls;
    for (pass = 0; pass < 4; ++pass) {
        int found_work = 0;
        int queue;

        for (queue = 7; queue >= 0; --queue) {
            uint32_t packed_count;
            unsigned int half_budget;
            unsigned int jumbo_budget;
            unsigned int normal_budget;
            int delivered;

            if ((scan_mask & (1U << queue)) == 0)
                continue;
            if (remaining <= 0)
                goto out;

            packed_count = cpu_net_ops->get_rx_count((unsigned int)queue);
            if (packed_count == 0) {
                if (queue != 7)
                    scan_mask &= ~(1U << queue);
                continue;
            }

            found_work = 1;
            half_budget = (unsigned int)remaining >> 1;
            jumbo_budget = packed_count >> 16;
            if (jumbo_budget > half_budget)
                jumbo_budget = half_budget;
            normal_budget = packed_count & 0xffffU;
            if (normal_budget > half_budget)
                normal_budget = half_budget;

            if (jumbo_budget != 0 &&
                (cpu_net_info_word_0 & (1U << (queue + 8))) != 0) {
                delivered = cpu_net_rx(jumbo_budget, (unsigned int)queue, 1);
                remaining -= delivered;
                processed += (unsigned int)delivered;
            }
            if (normal_budget != 0 &&
                (cpu_net_info_word_0 & (1U << queue)) != 0) {
                delivered = cpu_net_rx(normal_budget, (unsigned int)queue, 0);
                remaining -= delivered;
                processed += (unsigned int)delivered;
            }
        }

        if (!found_work)
            break;
    }

out:
    pp_tcp_gro_flush_all();
    if (remaining > 0) {
        napi_complete(&int_info[0]);
        cpu_net_ops->unmask_source(cpu_net_info_word_0);
        return (int)processed;
    }
    return budget;
}

int cpu_idm_poll(zte_napi_t *napi, int budget)
{
    int remaining = budget;
    unsigned int processed = 0;
    unsigned int scan_mask = 0xff;
    unsigned int pass;

    (void)napi;
    ++cpu_idm_poll_calls;
    for (pass = 0; pass < 4; ++pass) {
        int found_work = 0;
        int queue;

        for (queue = 7; queue >= 0; --queue) {
            uint32_t packed_count;
            unsigned int enabled_mask;
            unsigned int half_budget;
            unsigned int jumbo_budget;
            unsigned int normal_budget;
            int delivered;

            enabled_mask = (cpu_net_info_word_8 & 0xffU) |
                           ((cpu_net_info_word_8 >> 8) & 0xffU);
            if ((scan_mask & (1U << queue)) == 0 ||
                (enabled_mask & (1U << queue)) == 0)
                continue;
            if (remaining <= 0)
                return budget;

            packed_count = cpu_net_ops->get_rx_count((unsigned int)queue);
            if (packed_count == 0) {
                if (queue != 7)
                    scan_mask &= ~(1U << queue);
                continue;
            }

            found_work = 1;
            half_budget = (unsigned int)remaining >> 1;
            jumbo_budget = packed_count >> 16;
            if (jumbo_budget > half_budget)
                jumbo_budget = half_budget;
            normal_budget = packed_count & 0xffffU;
            if (normal_budget > half_budget)
                normal_budget = half_budget;

            if (jumbo_budget != 0 &&
                (cpu_net_info_word_8 & (1U << (queue + 8))) != 0) {
                delivered = cpu_net_rx(jumbo_budget, (unsigned int)queue, 1);
                remaining -= delivered;
                processed += (unsigned int)delivered;
            }
            if (normal_budget != 0 &&
                (cpu_net_info_word_8 & (1U << queue)) != 0) {
                delivered = cpu_net_rx(normal_budget, (unsigned int)queue, 0);
                remaining -= delivered;
                processed += (unsigned int)delivered;
            }
        }

        if (!found_work)
            break;
    }

    if (remaining <= 0)
        return budget;
    napi_complete(CPU_IDM_NAPI);
    cpu_net_ops->unmask_source(cpu_net_info_word_8);
    return (int)processed;
}

int idm_net_poll(zte_napi_t *napi, int budget)
{
    int remaining = budget;
    unsigned int processed = 0;
    unsigned int pass;

    (void)napi;
    ++idm_net_poll_calls;
    for (pass = 0; pass < 4; ++pass) {
        uint32_t packed_count = cpu_net_ops->get_rx_count(8);
        unsigned int half_budget;
        unsigned int jumbo_budget;
        unsigned int normal_budget;
        int delivered;

        if (remaining <= 0 || packed_count == 0)
            break;

        half_budget = (unsigned int)remaining >> 1;
        normal_budget = packed_count & 0xffffU;
        if (normal_budget > half_budget)
            normal_budget = half_budget;
        jumbo_budget = packed_count >> 16;
        if (jumbo_budget > half_budget)
            jumbo_budget = half_budget;

        if (jumbo_budget != 0) {
            delivered = idm_net_rx(jumbo_budget, 1);
            remaining -= delivered;
            processed += (unsigned int)delivered;
        }
        if (normal_budget != 0) {
            delivered = idm_net_rx(normal_budget, 0);
            remaining -= delivered;
            processed += (unsigned int)delivered;
        }
    }

    if (idm_recv_cmpl != 0)
        idm_recv_cmpl();
    if (remaining <= 0)
        return budget;
    napi_complete(IDM_NET_NAPI);
    cpu_net_ops->unmask_source(cpu_net_info_word_4);
    return (int)processed;
}

int cpu_rls_poll(zte_napi_t *napi, int budget)
{
    (void)napi;
    (void)budget;
    ++cpu_rls_poll_calls;
    do_raw_spin_lock((volatile uint32_t *)&idm_lock_tx);
    net_check_reorder_rls_nolock();
    /* The binary releases the queued spinlock's low byte with STLRB. */
    __atomic_store_n((uint8_t *)&idm_lock_tx, 0, __ATOMIC_RELEASE);
    napi_complete(CPU_RLS_NAPI);
    cpu_net_ops->unmask_source(cpu_net_info_word_c);
    return 1;
}

int cpu_net_rx(unsigned int count, unsigned int queue, unsigned int jumbo)
{
    unsigned int queue_index = queue + (jumbo << 3);
    void *rx_queue = cpu_net_ops->get_rx_queue(queue_index);
    zte_net_device_t *device = jumbo == 1 ? cpu_netdev : sw_netdev;
    unsigned int jumbo_seen = 0;
    unsigned int i;

    for (i = 0; i < count; ++i) {
        uint8_t *descriptor = get_next_rxdesc(rx_queue);
        uint32_t raw_buffer = RXD_U32(descriptor, 0);
        unsigned int pool = (RXD_U8(descriptor, 5) >> 6) & 1U;
        zte_netdev_stats_t *stats;

        if (raw_buffer == 0) {
            stats = cpu_dev_stat(device);
            if (stats != 0)
                ++stats->rx_dropped;
            cpu_net_ops->refill_rx_buffer(0, pool, 0);
            continue;
        }

        {
            unsigned int length = RXD_U16(descriptor, 4) & 0x3fffU;
            uint8_t *data = (uint8_t *)(uintptr_t)(
                ((uint64_t)raw_buffer - memstart_addr) |
                0xffffff8000000000ULL);

            ++net_rx_cnt[RXD_U8(descriptor, 6) & 0x3fU];

            if ((RXD_U8(descriptor, 5) & 0x80U) != 0) {
                stats = cpu_dev_stat(omcioam_netdev);
                if (cpu_omci_rx(descriptor, descriptor + 8, data, length) != 0) {
                    if (stats != 0)
                        ++stats->rx_dropped;
                } else if (stats != 0) {
                    ++stats->rx_packets;
                    stats->rx_bytes += length;
                }
                cpu_net_ops->refill_rx_buffer(raw_buffer, pool, 1);
                RXD_U32(descriptor, 0) = 0;
                continue;
            }

            if (cpu_net_ops->refill_rx_buffer(raw_buffer, pool, 0) < 0) {
                stats = cpu_dev_stat(device);
                if (stats != 0)
                    ++stats->rx_dropped;
                RXD_U32(descriptor, 0) = 0;
                continue;
            }

            stats = cpu_dev_stat(device);
            if (stats != 0) {
                ++stats->rx_packets;
                stats->rx_bytes += length;
            }

            {
                unsigned int payload_offset = RXD_U8(descriptor, 13);
                uint16_t embedded_length =
                    ((uint16_t)data[payload_offset + 2] << 8) |
                    data[payload_offset + 3];

                if (length - payload_offset >= embedded_length &&
                    RXD_U8(descriptor, 8) == 0xffU) {
                    testftp_net_report(data, descriptor + 8, length);
                    cpu_net_ops->free_buffer(
                        data - (uBP_BUFFER_OFFSET + 64U), pool);
                    RXD_U32(descriptor, 0) = 0;
                    continue;
                }
            }

            if ((RXD_U8(descriptor, 5) & 0x40U) != 0)
                ++jumbo_seen;

            if ((RXD_U8(descriptor, 14) & 0x15U) == 0x15U &&
                RXD_U8(descriptor, 8) != 0xfdU && pool == 0 &&
                switch_skb_recv != 0 && net_gro_en != 0) {
                ++net_gro_packet_count;
                if (pp_net_tcp_gro(descriptor, device, data, queue, jumbo) != 0)
                    continue;
            }

            pp_tcp_gro_flush_all();
            {
                unsigned int capacity = (pool != 0 ? uJUMBO_BP_SIZE :
                                         uNORMAL_BP_SIZE) - uSKB_SHAREDINFO_SIZE;
                zte_skb_t *skb = alloc_skb_attach_buffer(
                    0, data - (uBP_BUFFER_OFFSET + 64U), capacity,
                    uBP_BUFFER_OFFSET + 64U, raw_buffer);

                if (skb == 0) {
                    cpu_net_ops->free_buffer(
                        data - (uBP_BUFFER_OFFSET + 64U), pool);
                    RXD_U32(descriptor, 0) = 0;
                    if (stats != 0)
                        ++stats->rx_dropped;
                    continue;
                }

                skb_put(skb, length);
                SKB_PTR(skb, 0x10) = device;
                if (pool != 0)
                    SKB_U32(skb, 0x114) |= 2U;
                if (jumbo <= 1 && (RXD_U8(descriptor, 6) & 0x3fU) != 0)
                    SKB_U8(skb, 0x108) =
                        (RXD_U8(descriptor, 6) & 0x3fU) - 1U;

                if (switch_skb_recv != 0)
                    cpu_sw_rx(skb, device, descriptor, descriptor + 8, data,
                              queue);
                else {
                    SKB_U16(skb, 0xf8) = eth_type_trans(skb, device);
                    netif_receive_skb(skb);
                }
                RXD_U32(descriptor, 0) = 0;
            }
        }
    }

    cpu_net_ops->flush_rx_refill();
    cpu_net_ops->update_rx_queue(queue_index, count, jumbo_seen);
    return (int)count;
}

int idm_net_rx(unsigned int count, unsigned int jumbo)
{
    unsigned int queue_index = jumbo + 16U;
    void *rx_queue = cpu_net_ops->get_rx_queue(queue_index);
    unsigned int i;

    for (i = 0; i < count; ++i) {
        uint8_t *descriptor = get_next_rxdesc(rx_queue);
        uint32_t raw_buffer = RXD_U32(descriptor, 0);
        unsigned int length = RXD_U16(descriptor, 4) & 0x3fffU;
        uint8_t *data = (uint8_t *)(uintptr_t)(
            ((uint64_t)raw_buffer - memstart_addr) |
            0xffffff8000000000ULL);
        zte_netdev_stats_t *stats;

        if ((RXD_U8(descriptor, 5) & 0x40U) != 0) {
            ++idm_recv_jumbo_error;
            cpu_net_ops->refill_rx_buffer(raw_buffer, 1, 1);
            RXD_U32(descriptor, 0) = 0;
            stats = cpu_dev_stat(idm_netdev_object);
            if (stats != 0)
                ++stats->rx_dropped;
            continue;
        }

        if (cpu_net_ops->refill_rx_buffer(raw_buffer, 0, 0) < 0) {
            RXD_U32(descriptor, 0) = 0;
            stats = cpu_dev_stat(idm_netdev_object);
            if (stats != 0)
                ++stats->rx_dropped;
            continue;
        }

        {
            zte_skb_t *skb = alloc_skb_attach_buffer(
                net_alloc_skb(), data - (uBP_BUFFER_OFFSET + 64U),
                uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE,
                uBP_BUFFER_OFFSET + 64U, 0);

            if (skb == 0) {
                cpu_net_ops->free_buffer(
                    data - (uBP_BUFFER_OFFSET + 64U), 0);
                RXD_U32(descriptor, 0) = 0;
                stats = cpu_dev_stat(idm_netdev_object);
                if (stats != 0)
                    ++stats->rx_dropped;
                continue;
            }

            SKB_U32(skb, 0x114) |= 0x10000U;
            stats = cpu_dev_stat(idm_netdev_object);
            if (stats != 0) {
                ++stats->rx_packets;
                stats->rx_bytes += length;
            }
            skb_put(skb, length);
            SKB_PTR(skb, 0x10) = idm_netdev_object;
            if (idm_skb_recv != 0) {
                zte_wifi_trap_info_t trap_info;

                idm_set_wifi_trap_info(descriptor, &trap_info, 16);
                idm_skb_recv(&trap_info, skb);
            } else {
                SKB_U16(skb, 0xf8) = eth_type_trans(skb, idm_netdev_object);
                netif_receive_skb(skb);
            }
        }
    }

    cpu_net_ops->flush_rx_refill();
    cpu_net_ops->update_rx_queue(queue_index, count, 0);
    return (int)count;
}

int cpu_sw_rx(zte_skb_t *skb, zte_net_device_t *device, void *descriptor,
              void *metadata, const void *data, unsigned int queue)
{
    (void)device;
    (void)metadata;
    (void)data;

    if ((((unsigned int)RXD_U8(descriptor, 6) + 0x30U) & 0x3fU) <= 0x29U &&
        idm_skb_recv != 0) {
        zte_wifi_trap_info_t trap_info;

        ++cpu_sw_trap_count;
        SKB_PTR(skb, 0x10) = idm_netdev_object;
        idm_set_wifi_trap_info(descriptor, &trap_info, queue);
        idm_skb_recv(&trap_info, skb);
    } else {
        switch_skb_recv(skb);
    }

    return 0;
}

int cpu_omci_rx(void *descriptor, void *metadata, const void *data,
                unsigned int length)
{
    unsigned int port;

    (void)descriptor;
    if (omci_oam_rx == 0)
        return 0;

    port = RXD_U16(metadata, 2);
    if ((g_pon_work_mode & 0xe40U) != 0) {
        if (omci_mic_check != 0 && omci_mic_check(data, length) != 0)
            return -1;
        local_omci_port_id = port;
        omci_oam_rx(data, length, 0);
    } else {
        if (port > 7U)
            port = 0;
        omci_oam_rx(data, length, port);
    }

    return 0;
}

static uint16_t gro_load_u16(const uint8_t *pointer)
{
    return *(const uint16_t *)(const void *)pointer;
}

static uint16_t gro_be16(const uint8_t *pointer)
{
    uint16_t value = gro_load_u16(pointer);

    return (uint16_t)((value << 8) | (value >> 8));
}

static uint32_t gro_load_u32(const uint8_t *pointer)
{
    return *(const uint32_t *)(const void *)pointer;
}

uint32_t __fswab32_0(uint32_t value)
{
    return ((value & 0x000000ffU) << 24) |
           ((value & 0x0000ff00U) << 8) |
           ((value & 0x00ff0000U) >> 8) |
           ((value & 0xff000000U) >> 24);
}

uint32_t sub_10738(void)
{
    uint64_t priority_mask = read_icc_pmr_el1();

    write_icc_pmr_el1((uint32_t)priority_mask ^ 0xe0U);
    write_icc_pmr_el1(priority_mask);
    dsb_sy();
    return __fswab32_0((uint32_t)read_tpidr_el2());
}

static uint32_t gro_be32(const uint8_t *pointer)
{
    return __fswab32_0(gro_load_u32(pointer));
}

static uint32_t gro_ror32(uint32_t value, unsigned int shift)
{
    return (value >> shift) | (value << (32U - shift));
}

static uint32_t gro_flow_hash(const uint8_t *ipv4, const uint8_t *tcp)
{
    uint32_t a;
    uint32_t b;
    uint32_t c;

    a = (uint32_t)gro_load_u16(tcp + 2) |
        ((uint32_t)gro_load_u16(tcp) << 16);
    b = (uint32_t)*(const uint32_t *)(const void *)(ipv4 + 16) |
        ((uint32_t)*(const uint32_t *)(const void *)(ipv4 + 12) << 16);
    c = 0xbd5b7de6U;
    a += c;
    b += c;
    c = (a ^ c) - gro_ror32(a, 18);
    b = (b ^ c) - gro_ror32(c, 21);
    a = (a ^ b) - gro_ror32(b, 7);
    c = (c ^ a) - gro_ror32(a, 16);
    b = (b ^ c) - gro_ror32(c, 28);
    a = (a ^ b) - gro_ror32(b, 18);
    c = (c ^ a) - gro_ror32(a, 8);
    return c;
}

static unsigned int gro_bucket(uint32_t hash)
{
    return (uint32_t)(1640531527U * hash) >> 28;
}

static zte_gro_flow_t *gro_flow_from_node(zte_gro_hlist_node_t *node)
{
    return (zte_gro_flow_t *)((uint8_t *)node - 0x10U);
}

static zte_gro_flow_t *gro_find_exact_flow(uint32_t hash,
                                             const uint8_t *ipv4,
                                             const uint8_t *tcp,
                                             const uint8_t *descriptor)
{
    zte_gro_hlist_node_t *node;
    unsigned int port = RXD_U8(descriptor, 6) & 0x3fU;

    for (node = gro_hash_table[gro_bucket(hash)]; node != 0; node = node->next) {
        zte_gro_flow_t *flow = gro_flow_from_node(node);
        zte_skb_t *skb;
        const uint8_t *old_ipv4;
        const uint8_t *old_tcp;

        if (flow->hash != hash)
            continue;
        skb = flow->skb;
        if (SKB_U8(skb, 0x58) != port)
            continue;
        old_ipv4 = SKB_PTR(skb, 0x30);
        old_tcp = SKB_PTR(skb, 0x38);
        if (*(const uint32_t *)(const void *)(old_ipv4 + 12) !=
                *(const uint32_t *)(const void *)(ipv4 + 12) ||
            *(const uint32_t *)(const void *)(old_ipv4 + 16) !=
                *(const uint32_t *)(const void *)(ipv4 + 16) ||
            *(const uint32_t *)(const void *)old_tcp !=
                *(const uint32_t *)(const void *)tcp)
            continue;
        return flow;
    }

    return 0;
}

int search_gro_flow(uint32_t hash)
{
    zte_gro_hlist_node_t *node;

    for (node = gro_hash_table[gro_bucket(hash)]; node != 0; node = node->next) {
        if (gro_flow_from_node(node)->hash == hash)
            return 1;
    }

    return 0;
}

static void gro_add_flow(zte_gro_flow_t *flow, unsigned int bucket)
{
    zte_gro_hlist_node_t *head = gro_hash_table[bucket];

    flow->node.next = head;
    if (head != 0)
        head->pprev = &flow->node.next;
    gro_hash_table[bucket] = &flow->node;
    flow->node.pprev = &gro_hash_table[bucket];
}

static uint64_t gro_encode_fragment_page(const uint8_t *payload)
{
    return ((((uint64_t)(uintptr_t)payload + 0x8000000000ULL) >> 12) << 6) -
           0x100200000ULL;
}

int can_tcp_gro(zte_skb_t *flow_skb, const uint8_t *ipv4,
                const uint8_t *tcp, const uint8_t *descriptor)
{
    uint32_t idm_pool_end;
    uint64_t virtual_limit;
    unsigned int payload_length;
    const uint8_t *old_ipv4;
    const uint8_t *old_tcp;

    idm_pool_end = idm_reserved_base + 0x800U +
                   4U * (uIDM_RX_NORMAL_BP_NUM + uIDM_RX_JUMBO_BP_NUM +
                         uIDM_TX_JUMBO_BP_RETRV_NUM +
                         uIDM_TX_NORMAL_BP_RETRV_NUM +
                         uIDM_TX_EXTRAL_BP_RETRV_NUM) +
                   128U * (uIDM_TX_QUEUE_DESC_DEPTH +
                            6U * uIDM_RX_QUEUE_DESC_DEPTH);
    virtual_limit = ((uint64_t)idm_pool_end - memstart_addr) |
                    0xffffff8000000000ULL;
    if ((uint64_t)(uintptr_t)ipv4 < virtual_limit)
        return 0;
    if ((gro_load_u32(tcp + 12) & 0x2f00U) != 0)
        return 0;

    payload_length = gro_be16(ipv4 + 2) - 4U * (ipv4[0] & 0x0fU) -
                     4U * ((tcp[12] >> 4) & 0x0fU);
    if (g_cur_flows <= 15U && flow_skb == 0)
        return payload_length > 0x54fU;

    if (SKB_U8(flow_skb, 0x58) != (RXD_U8(descriptor, 6) & 0x3fU))
        return 0;
    old_ipv4 = SKB_PTR(flow_skb, 0x30);
    if (gro_load_u32(old_ipv4 + 12) != gro_load_u32(ipv4 + 12) ||
        gro_load_u32(old_ipv4 + 16) != gro_load_u32(ipv4 + 16))
        return 0;
    old_tcp = SKB_PTR(flow_skb, 0x38);
    if (gro_load_u32(old_tcp) != gro_load_u32(tcp))
        return 0;

    return gro_be32(old_tcp + 4) + SKB_U32(flow_skb, 0xac) +
               payload_length == gro_be32(tcp + 4) &&
           gro_load_u32(old_tcp + 8) == gro_load_u32(tcp + 8);
}

int is_l4port_supported(uint16_t port, unsigned int destination)
{
    zte_gro_port_list_t *list =
        destination != 0 ? &supported_dest_ports : &supported_source_ports;
    zte_gro_port_node_t *node;
    int found = 0;

    __raw_spin_lock_bh_constprop_13();
    for (node = list->head.next; node != &list->head; node = node->next) {
        zte_gro_port_entry_t *entry =
            (zte_gro_port_entry_t *)((uint8_t *)node - 8U);

        if (entry->port == port) {
            found = 1;
            break;
        }
    }
    __atomic_store_n((uint8_t *)&groport_busy_lock, 0, __ATOMIC_RELEASE);
    __local_bh_enable_ip((uintptr_t)__builtin_return_address(0), 512U);
    return found;
}

void __raw_spin_lock_bh_constprop_13(void)
{
    uint8_t *current = (uint8_t *)(uintptr_t)read_sp_el0();
    uint32_t observed;

    *(uint32_t *)(void *)(current + 0x10) += 0x200U;
    for (;;) {
        uint32_t expected = 0;

        observed = __atomic_load_n(&groport_busy_lock, __ATOMIC_ACQUIRE);
        if (observed != 0)
            break;
        if (__atomic_compare_exchange_n(&groport_busy_lock, &expected, 1, 0,
                                        __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED))
            return;
    }

    queued_spin_lock_slowpath(&groport_busy_lock, observed, 0, 1);
}

int add_supported_l4port(uint16_t port, unsigned int destination)
{
    zte_gro_port_entry_t *entry =
        kmem_cache_alloc(gro_state_cache, 0xcc0U);
    zte_gro_port_list_t *list;
    zte_gro_port_node_t *tail;

    if (entry == 0) {
        printk(add_supported_l4port_alloc_failure);
        return -1;
    }

    entry->port = port;
    entry->opaque_2[0] = 0;
    entry->opaque_2[1] = 0;
    entry->opaque_2[2] = 0;
    entry->opaque_2[3] = 0;
    entry->opaque_2[4] = 0;
    entry->opaque_2[5] = 0;
    entry->node.next = 0;
    entry->node.prev = 0;

    __raw_spin_lock_bh_constprop_13();
    list = destination != 0 ? &supported_dest_ports : &supported_source_ports;
    tail = list->head.prev;
    list->head.prev = &entry->node;
    entry->node.next = &list->head;
    entry->node.prev = tail;
    tail->next = &entry->node;
    __atomic_store_n((uint8_t *)&groport_busy_lock, 0, __ATOMIC_RELEASE);
    __local_bh_enable_ip((uintptr_t)__builtin_return_address(0), 512U);
    return 1;
}

int remove_supported_l4port(uint16_t port, unsigned int destination)
{
    zte_gro_port_list_t *list =
        destination != 0 ? &supported_dest_ports : &supported_source_ports;
    zte_gro_port_node_t *node;

    __raw_spin_lock_bh_constprop_13();
    for (node = list->head.next; node != &list->head; node = node->next) {
        zte_gro_port_entry_t *entry =
            (zte_gro_port_entry_t *)((uint8_t *)node - 8U);

        if (entry->port != port)
            continue;

        node->next->prev = node->prev;
        node->prev->next = node->next;
        node->next = (zte_gro_port_node_t *)(uintptr_t)0xdead000000000100ULL;
        node->prev = (zte_gro_port_node_t *)(uintptr_t)0xdead000000000122ULL;
        kfree(entry);
        break;
    }
    __atomic_store_n((uint8_t *)&groport_busy_lock, 0, __ATOMIC_RELEASE);
    __local_bh_enable_ip((uintptr_t)__builtin_return_address(0), 512U);
    return 1;
}

void pp_tcp_gro_flush(zte_skb_t *skb)
{
    uint8_t *shared = (uint8_t *)SKB_PTR(skb, 0x128) + SKB_U32(skb, 0x120);

    if (shared[2] != 0) {
        uint8_t *ipv4 = SKB_PTR(skb, 0x30);
        uint16_t total_length =
            (uint16_t)(gro_be16(ipv4 + 2) + SKB_U32(skb, 0xac));
        uint8_t *outer_ipv4 = SKB_PTR(skb, 0x40);

        *(uint16_t *)(void *)(ipv4 + 2) =
            (uint16_t)((total_length << 8) | (total_length >> 8));
        if (outer_ipv4 != 0) {
            total_length =
                (uint16_t)(gro_be16(outer_ipv4 + 4) + SKB_U32(skb, 0xac));
            *(uint16_t *)(void *)(outer_ipv4 + 4) =
                (uint16_t)((total_length << 8) | (total_length >> 8));
        }
        ip_send_check(ipv4);
    }

    if ((uint8_t)(SKB_U8(skb, 0x108) - 16U) <= 32U && idm_skb_recv != 0) {
        zte_wifi_trap_info_t trap_info = { 0 };

        SKB_PTR(skb, 0x10) = wifi_gro_netdev;
        idm_set_wifi_trap_info(wifi_gro_desc, &trap_info, wifi_gro_rxq);
        idm_skb_recv(&trap_info, skb);
    } else {
        switch_skb_recv(skb);
    }

    ++net_gro_cnt;
    net_gro_segment_count += (uint32_t)shared[2] + 1U;
    if (net_smb_state != 1U)
        net_smb_state = 1;
    ++net_gro_flush_count;
}

void pp_tcp_gro_flush_all(void)
{
    unsigned int bucket;

    for (bucket = 0; bucket < 16; ++bucket) {
        zte_gro_hlist_node_t *node = gro_hash_table[bucket];

        while (node != 0) {
            zte_gro_hlist_node_t *next = node->next;
            zte_gro_flow_t *flow = gro_flow_from_node(node);

            pp_tcp_gro_flush(flow->skb);
            hlist_del_init(&flow->node);
            --g_cur_flows;
            kfree(flow);
            node = next;
        }
    }
}

void net_gro_init(void)
{
    pp_smb_test_config = lower_net_smb_test_config;
}

static const unsigned long *gro_cpu_bit_mask(unsigned int cpu)
{
    return (const unsigned long *)((const uint8_t *)cpu_bit_bitmap +
                                   8U * (cpu & 0x3fU) + 8U -
                                   8U * (cpu >> 6));
}

int lower_net_smb_test_config(int value)
{
    unsigned int result;

    if (value != 0) {
        net_gro_en = 3;
        printk("lower_net_smb_test_config start\n");
        result = 1;
    } else {
        printk("lower_net_smb_test_config end\n");
        net_gro_en = 0;
        net_smb_state = 0;
        irq_set_affinity_hint(g_idm_irq[0],
                              gro_cpu_bit_mask(g_idm_irq_to_cpu));
        result = 2;
    }

    g_net_check_threshold = (uint16_t)result;
    return (int)result;
}

void net_gso_init(void)
{
    zte_proc_dir_t *directory;

    g_upload_driver_en = 1;
    directory = proc_mkdir("upload_ctl", 0);
    if (directory == 0) {
        if (__printk_ratelimit("upload_test_proc_init"))
            printk("\n Can't create /proc/upload_ctl\n");
    } else if (proc_create("upload", 128U, directory, upload_test_fops) == 0 &&
               __printk_ratelimit("upload_test_proc_init")) {
        printk("create porc/upload_ctl/upload!\n");
    }

    upload_hook = net_upload_fun;
}

void net_upload_fun(int value)
{
    uint8_t *current;
    uint32_t observed;

    if (net_gso_debug > 0) {
        printk("net_upload_fun en %u\n", (unsigned int)value);
        --net_gso_debug;
    }

    current = (uint8_t *)(uintptr_t)read_sp_el0();
    *(uint32_t *)(void *)(current + 0x10) += 0x200U;
    for (;;) {
        uint32_t expected = 0;

        observed = __atomic_load_n(&net_lock_tx, __ATOMIC_ACQUIRE);
        if (observed != 0)
            break;
        if (__atomic_compare_exchange_n(&net_lock_tx, &expected, 1, 0,
                                        __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED))
            break;
    }
    if (observed != 0)
        queued_spin_lock_slowpath(&net_lock_tx, observed,
                                  (uintptr_t)net_lock_slowpath_context, 0);

    if (value != 0) {
        if (upload_count == 0)
            gso_upload_enable();
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        ++upload_count;
    } else {
        if (upload_count > 0)
            --upload_count;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (upload_count == 0)
            gso_upload_disable(0);
    }

    __atomic_store_n((uint8_t *)&net_lock_tx, 0, __ATOMIC_RELEASE);
    __local_bh_enable_ip((uintptr_t)__builtin_return_address(0), 512U);
}

long upload_write_proc(void *file, const char *user_buffer, unsigned long count)
{
    char input[8];

    (void)file;
    input[0] = 0;
    if (!upload_user_range_ok(user_buffer, sizeof(input)))
        return -1;
    if (__arch_copy_from_user(input, user_buffer, sizeof(input)) != 0)
        return -1;
    if (input[0] != 0)
        net_upload_fun((uint8_t)simple_strtoul(input, 0, 10));
    return (long)count;
}

void gso_upload_enable(void)
{
    unsigned int index;

    if (gso_buf_cnt != 0)
        return;

    for (index = 0; index < 64; ++index) {
        uint8_t *nbuf = cpu_net_alloc_nbuf();

        if (nbuf == 0) {
            if (__printk_ratelimit("gso_upload_enable"))
                printk("gso upload alloc buf failed\n");
            gso_upload_disable(1);
            return;
        }

        nbuf[0x2c] |= 2U;
        memset(*(void **)(void *)(nbuf + 0x18), 0,
               uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE - uBP_BUFFER_OFFSET -
                   64U);
        gso_nbuf_pool[index] = nbuf;
        s_gso_last_hlen[index] = 0;
    }

    gso_buf_cnt = 64;
    gso_buf_idx = 0;
}

void gso_upload_disable(unsigned int release_buffers)
{
    unsigned int index;

    for (index = 0; index < gso_buf_cnt; ++index) {
        uint8_t *nbuf = gso_nbuf_pool[index];

        if (nbuf == 0)
            continue;
        if (release_buffers != 0) {
            nbuf[0x2c] &= (uint8_t)~2U;
            cpu_net_free_nbuf(nbuf);
            gso_nbuf_pool[index] = 0;
        } else {
            memset(*(void **)(void *)(nbuf + 0x18), 0,
                   uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE -
                       uBP_BUFFER_OFFSET - 64U);
        }
        s_gso_last_hlen[index] = 0;
    }

    if (release_buffers != 0)
        gso_buf_cnt = 0;
}

void *cpu_net_alloc_nbuf(void)
{
    return cpu_net_ops->alloc_buffer();
}

void cpu_net_free_nbuf(void *nbuf)
{
    uint8_t *raw_nbuf = nbuf;

    if ((raw_nbuf[0x2c] & 2U) == 0) {
        void *buffer = *(void **)(void *)(raw_nbuf + 0x10);

        cpu_net_ops->free_buffer((uint8_t *)buffer - 64U, 0);
    } else {
        ++g_nb_not_rls_cnt;
    }
}

static unsigned int vendor_udiv32(unsigned int dividend, unsigned int divisor)
{
    return divisor == 0 ? 0 : dividend / divisor;
}

static uint16_t vendor_bswap16(uint16_t value)
{
    return (uint16_t)((value << 8) | (value >> 8));
}

static uint8_t *gso_fragment_virtual_data(const uint8_t *fragment)
{
    uint64_t page_link = *(const uint64_t *)(const void *)fragment;
    uint32_t page_offset = *(const uint32_t *)(const void *)(fragment + 12);

    return (uint8_t *)(uintptr_t)(0xffffff8000000000ULL +
        (((page_link + 0x100200000ULL) >> 6) << 12) + page_offset);
}

static uint16_t vendor_csum_fold(uint32_t sum)
{
    sum = (sum & 0xffffU) + (sum >> 16);
    sum = (sum & 0xffffU) + (sum >> 16);
    return (uint16_t)~sum;
}

int net_gso_upload_send(void *nbuf, zte_skb_t *skb,
                        unsigned int payload_length,
                        unsigned int gso_segment_size)
{
    uint8_t *raw_nbuf = nbuf;
    uint8_t *descriptor = net_get_next_txdesc(cpu_tq);
    uint16_t descriptor_control;
    uint16_t nbuf_length;

    if (descriptor == 0) {
        cpu_net_free_nbuf(nbuf);
        ++gso_upload_desc_unavailable;
        return -1;
    }

    net_cfg_desc_by_skb(descriptor, skb, raw_nbuf[0x2c] & 1U);
    *(uint32_t *)(void *)descriptor =
        (uint32_t)virt_to_phys(*(void **)(void *)(raw_nbuf + 0x18));
    descriptor_control = *(uint16_t *)(void *)(descriptor + 4);
    nbuf_length = *(uint16_t *)(void *)(raw_nbuf + 0x28);
    descriptor_control = (uint16_t)((descriptor_control & 0x8001U) |
                                    ((nbuf_length & 0x3fffU) << 1));
    *(uint16_t *)(void *)(descriptor + 4) = descriptor_control;
    *(uint32_t *)(void *)(descriptor + 16) = 0;
    *(uint32_t *)(void *)(descriptor + 20) = 0;
    descriptor[27] |= 0x0cU;

    if (payload_length > gso_segment_size) {
        unsigned int segment_count =
            vendor_udiv32(payload_length, gso_segment_size);
        unsigned int remainder =
            payload_length - segment_count * gso_segment_size;

        *(uint16_t *)(void *)(descriptor + 22) =
            (uint16_t)gso_segment_size;
        if (remainder != 0) {
            *(uint16_t *)(void *)(descriptor + 18) =
                (uint16_t)segment_count;
            *(uint16_t *)(void *)(descriptor + 20) =
                (uint16_t)remainder;
        } else {
            *(uint16_t *)(void *)(descriptor + 18) =
                (uint16_t)(segment_count - 1U);
            *(uint16_t *)(void *)(descriptor + 20) =
                (uint16_t)gso_segment_size;
        }
    }

    ++gso_upload_send_count;
    dsb_st();
    return cpu_net_nb_desc_tx(nbuf, descriptor);
}

int net_tcp_gso_tx_upload(zte_skb_t *skb, zte_net_device_t *device,
                          unsigned int path)
{
    uint8_t *shared_info = (uint8_t *)SKB_PTR(skb, 0x128) +
                           SKB_U16(skb, 0xfa);
    unsigned int gso_segment_size =
        *(uint16_t *)(void *)((uint8_t *)SKB_PTR(skb, 0x128) +
                              SKB_U32(skb, 0x120) + 4);
    uint8_t *network_header = network_hdr_optimized();
    uint8_t *packet_data = SKB_PTR(skb, 0x130);
    unsigned int ip_version = network_header[0] >> 4;
    unsigned int network_offset =
        (unsigned int)((uintptr_t)network_header - (uintptr_t)packet_data);
    unsigned int tcp_header_length = 4U * (shared_info[12] >> 4);
    unsigned int header_length = tcp_header_length +
        (unsigned int)((uintptr_t)shared_info - (uintptr_t)packet_data);
    unsigned int payload_length = SKB_U32(skb, 0xa8) - header_length;
    unsigned int first_payload_length;
    uint8_t *nbuf;
    uint8_t *nbuf_data;
    uint8_t *tcp_header;
    uint16_t nbuf_length;

    (void)device;
    if (gso_segment_size == 0)
        gso_segment_size = 1514U - header_length;
    first_payload_length = payload_length > gso_segment_size ?
        gso_segment_size : payload_length;
    ++gso_upload_attempt_count;

    if (net_gso_debug > 0)
        --net_gso_debug;

    if (network_offset > 0x15U &&
        *(uint16_t *)(void *)(packet_data + network_offset - 10U) == 0x6488U) {
        *(uint16_t *)(void *)(network_header - 2) = vendor_bswap16(
            (uint16_t)(header_length + 2U + first_payload_length -
                       network_offset));
    }

    if (gso_buf_cnt != 0) {
        unsigned int index = gso_buf_idx;
        unsigned int previous_header_length;

        nbuf = gso_nbuf_pool[index];
        gso_buf_idx = ((uint8_t)gso_buf_idx + 1U) & 0x3fU;
        previous_header_length = s_gso_last_hlen[index];
        if (header_length < previous_header_length) {
            /* The binary dereferences the selected slot before its null check. */
            memset((uint8_t *)*(void **)(void *)(nbuf + 0x18) +
                       header_length, 0,
                   previous_header_length - header_length);
        }
        s_gso_last_hlen[index] = header_length;
        if (nbuf == 0)
            goto no_nbuf;
    } else {
        nbuf = cpu_net_alloc_nbuf();
        if (nbuf == 0)
            goto no_nbuf;
        memset((uint8_t *)*(void **)(void *)(nbuf + 0x18) + header_length, 0,
               first_payload_length);
    }

    nbuf_data = *(uint8_t **)(void *)(nbuf + 0x18);
    nbuf_length = (uint16_t)(header_length + first_payload_length);
    memcpy(nbuf_data, packet_data, header_length);
    tcp_header = nbuf_data + header_length - tcp_header_length;
    *(uint16_t *)(void *)(nbuf + 0x28) = nbuf_length;
    nbuf[0x2c] = (nbuf[0x2c] & 0xfeU) | (path & 1U);

    if (ip_version == 4) {
        uint8_t *ipv4 = nbuf_data + network_offset;

        ++gso_upload_ipv4_count;
        *(uint16_t *)(void *)(ipv4 + 2) =
            vendor_bswap16((uint16_t)(nbuf_length - network_offset));
        net_gso_checksum_upload(ipv4, tcp_header, 1);
    } else if (ip_version == 6) {
        uint8_t *ipv6 = nbuf_data + network_offset;
        uint16_t ipv6_payload_length =
            (uint16_t)(first_payload_length + tcp_header_length);

        ++gso_upload_ipv6_count;
        *(uint16_t *)(void *)(ipv6 + 4) =
            vendor_bswap16(ipv6_payload_length);
        if (ipv6[6] == 4) {
            uint16_t nested_length =
                vendor_bswap16((uint16_t)(ipv6_payload_length + 20U));
            uint8_t *inner_ipv4 = ipv6 + 40;

            *(uint16_t *)(void *)(ipv6 + 4) = nested_length;
            *(uint16_t *)(void *)(inner_ipv4 + 2) = nested_length;
            *(uint16_t *)(void *)(inner_ipv4 + 10) = 0;
            net_gso_checksum_upload(inner_ipv4, tcp_header, 1);
        } else {
            net_gso_ipv6tcp_checksum_constprop_6(ipv6, tcp_header);
        }
    }

    return net_gso_upload_send(nbuf, skb, payload_length, gso_segment_size);

no_nbuf:
    ++gso_upload_nbuf_unavailable;
    return -1;
}

int net_tcp_gso_tx_upload1(zte_skb_t *skb, zte_net_device_t *device,
                           unsigned int path)
{
    uint8_t *transport_header = (uint8_t *)SKB_PTR(skb, 0x128) +
                                SKB_U16(skb, 0xfa);
    unsigned int gso_segment_size =
        *(uint16_t *)(void *)((uint8_t *)SKB_PTR(skb, 0x128) +
                              SKB_U32(skb, 0x120) + 4);
    uint8_t *network_header = network_hdr_optimized();
    uint8_t *packet_data = SKB_PTR(skb, 0x130);
    unsigned int network_offset =
        (unsigned int)((uintptr_t)network_header - (uintptr_t)packet_data);
    unsigned int l3_header_length =
        (unsigned int)((uintptr_t)transport_header -
                       (uintptr_t)network_header);
    unsigned int tcp_header_length = 4U * (transport_header[12] >> 4);
    unsigned int header_length = tcp_header_length +
        (unsigned int)((uintptr_t)transport_header - (uintptr_t)packet_data);
    unsigned int remaining_payload = SKB_U32(skb, 0xa8) - header_length;
    unsigned int ip_version = network_header[0] >> 4;
    uint32_t tcp_sequence =
        __fswab32_0(*(uint32_t *)(void *)(transport_header + 4));

    (void)device;
    if (gso_segment_size == 0)
        gso_segment_size = 1514U - header_length;
    ++gso_upload1_attempt_count;
    if (net_gso_debug > 0)
        --net_gso_debug;

    if (network_offset > 0x15U &&
        *(uint16_t *)(void *)(packet_data + network_offset - 10U) == 0x6488U) {
        unsigned int first_payload = remaining_payload > gso_segment_size ?
            gso_segment_size : remaining_payload;

        *(uint16_t *)(void *)(network_header - 2) = vendor_bswap16(
            (uint16_t)(first_payload - network_offset + header_length + 2U));
    }

    while (remaining_payload != 0) {
        uint8_t *nbuf = cpu_net_alloc_nbuf();
        unsigned int segment_length;
        uint8_t *nbuf_data;
        uint8_t *tcp_header;

        if (nbuf == 0) {
            ++gso_upload_nbuf_unavailable;
            return -1;
        }

        segment_length = remaining_payload > gso_segment_size ?
            gso_segment_size : remaining_payload;
        nbuf_data = *(uint8_t **)(void *)(nbuf + 0x18);
        memset(nbuf_data + header_length, 0, segment_length);
        memcpy(nbuf_data, packet_data, header_length);
        nbuf[0x2c] = (nbuf[0x2c] & 0xfeU) | (path & 1U);
        *(uint16_t *)(void *)(nbuf + 0x28) =
            (uint16_t)(header_length + segment_length);

        tcp_header = nbuf_data + header_length - tcp_header_length;
        *(uint32_t *)(void *)(tcp_header + 4) = __fswab32_0(tcp_sequence);
        tcp_sequence += segment_length;

        if (ip_version == 4) {
            uint8_t *ipv4 = tcp_header - l3_header_length;

            ++gso_upload_ipv4_count;
            *(uint16_t *)(void *)(ipv4 + 2) = vendor_bswap16(
                (uint16_t)(header_length + segment_length));
            net_gso_checksum_upload(ipv4, tcp_header, 1);
        } else if (ip_version == 6) {
            uint8_t *ipv6 = tcp_header - l3_header_length;
            uint16_t ipv6_payload_length =
                (uint16_t)(segment_length + tcp_header_length);

            ++gso_upload_ipv6_count;
            *(uint16_t *)(void *)(ipv6 + 4) =
                vendor_bswap16(ipv6_payload_length);
            if (ipv6[6] == 4) {
                uint16_t nested_length = vendor_bswap16(
                    (uint16_t)(ipv6_payload_length + 20U));
                uint8_t *inner_ipv4 = tcp_header +
                    (40U - l3_header_length);

                *(uint16_t *)(void *)(ipv6 + 4) = nested_length;
                *(uint16_t *)(void *)(inner_ipv4 + 2) = nested_length;
                *(uint16_t *)(void *)(inner_ipv4 + 10) = 0;
                net_gso_checksum_upload(inner_ipv4, tcp_header, 1);
            } else {
                net_gso_ipv6tcp_checksum_constprop_6(ipv6, tcp_header);
            }
        }

        remaining_payload -= segment_length;
        if (net_gso_upload_send(nbuf, skb, segment_length, segment_length) < 0)
            return -1;
    }

    return 0;
}

int net_tcp_gso_tx(zte_skb_t *skb, zte_net_device_t *device,
                   unsigned int direction)
{
    uint8_t *head = SKB_PTR(skb, 0x128);
    uint8_t *data = SKB_PTR(skb, 0x130);
    uint8_t *tcp = head + SKB_U16(skb, 0xfa);
    uint8_t *shared_info = head + SKB_U32(skb, 0x120);
    int tcp_offset = (int)((intptr_t)tcp - (intptr_t)data);
    int header_length = tcp_offset + 4 * (tcp[12] >> 4);
    int remaining_payload = (int)SKB_U32(skb, 0xa8) - header_length;
    int linear_length = (int)SKB_U32(skb, 0xa8) - (int)SKB_U32(skb, 0xac);
    uint16_t ipv4_id = vendor_bswap16(
        *(uint16_t *)(void *)(tcp - 16));
    uint32_t tcp_sequence = __fswab32_0(
        *(uint32_t *)(void *)(tcp + 4));
    unsigned int saved_psh = (tcp[13] >> 3) & 1U;
    uint8_t *source;
    int source_available;
    unsigned int next_fragment;

    (void)device;
    ++gso_normal_attempt_count;
    tcp[13] &= (uint8_t)~8U;
    if (header_length > linear_length) {
        printk("***** ERROR: gso frag_size=%d, hlen=%d\n", linear_length,
               header_length);
        return -3;
    }

    if (linear_length == header_length) {
        source = gso_fragment_virtual_data(shared_info + 0x30);
        source_available = *(uint32_t *)(void *)(shared_info + 0x38);
        next_fragment = 1;
    } else {
        source = data + header_length;
        source_available = linear_length - header_length;
        next_fragment = 0;
    }

    if (*(uint16_t *)(void *)(shared_info + 4) == 0)
        *(uint16_t *)(void *)(shared_info + 4) =
            (uint16_t)(header_length + 0x57eU);

    while (remaining_payload > 0) {
        unsigned int segment_length = remaining_payload <
            *(uint16_t *)(void *)(shared_info + 4) ?
            (unsigned int)remaining_payload :
            *(uint16_t *)(void *)(shared_info + 4);
        uint8_t *nbuf = cpu_net_alloc_nbuf();
        uint8_t *nbuf_data;
        uint8_t *ipv4;
        uint8_t *out_tcp;
        int ip_offset;
        uint16_t nbuf_length;
        uint16_t ipv4_length;
        uint32_t next_sequence;
        unsigned int payload_left;
        uint8_t *descriptor;

        if (nbuf == 0) {
            ++gso_upload_nbuf_unavailable;
            return -1;
        }

        remaining_payload -= (int)segment_length;
        nbuf_data = *(uint8_t **)(void *)(nbuf + 0x18);
        memcpy(nbuf_data, data, (unsigned int)header_length);
        ip_offset = tcp_offset - 20;
        ipv4 = nbuf_data + ip_offset;
        out_tcp = nbuf_data + tcp_offset;
        nbuf_length = (uint16_t)(segment_length + header_length);
        ipv4_length = (uint16_t)(nbuf_length - (uint16_t)ip_offset);

        *(uint16_t *)(void *)(ipv4 + 4) = vendor_bswap16(ipv4_id++);
        *(uint16_t *)(void *)(ipv4 + 2) = vendor_bswap16(ipv4_length);
        if (ip_offset > 21 &&
            *(uint16_t *)(void *)(nbuf_data +
                (uint16_t)(ip_offset - 10)) == 0x6488U) {
            *(uint16_t *)(void *)(ipv4 - 4) =
                vendor_bswap16((uint16_t)(ipv4_length + 2U));
        }

        *(uint32_t *)(void *)(out_tcp + 4) = __fswab32_0(tcp_sequence);
        next_sequence = tcp_sequence + segment_length;
        if (remaining_payload == 0)
            out_tcp[13] = (out_tcp[13] & (uint8_t)~8U) |
                          (uint8_t)(saved_psh << 3);

        payload_left = segment_length;
        while (payload_left != 0) {
            unsigned int copy_length = payload_left < (unsigned int)source_available ?
                payload_left : (unsigned int)source_available;

            memcpy(nbuf_data + header_length + (segment_length - payload_left),
                   source, copy_length);
            source += copy_length;
            source_available -= (int)copy_length;
            payload_left -= copy_length;
            if (source_available != 0 || payload_left == 0)
                continue;

            if (next_fragment >= shared_info[2]) {
                printk("gso tx bug!!frag %d/%d\n", next_fragment,
                       shared_info[2]);
                source_available = 0;
                remaining_payload = 0;
                break;
            }

            source = gso_fragment_virtual_data(shared_info + 0x30 +
                                                0x10U * next_fragment);
            source_available = *(uint32_t *)(void *)(shared_info + 0x30 +
                0x10U * next_fragment + 8);
            ++next_fragment;
        }

        *(uint16_t *)(void *)(nbuf + 0x28) = nbuf_length;
        nbuf[0x2c] = (nbuf[0x2c] & 0xfeU) | (direction & 1U);

        if (net_hw_checksum != 0) {
            ip_send_check(ipv4);
            *(uint16_t *)(void *)(out_tcp + 16) = 0;
        } else {
            net_gso_checksum_upload(ipv4, out_tcp, 0);
        }

        descriptor = net_get_next_txdesc(cpu_tq);
        if (descriptor == 0) {
            cpu_net_free_nbuf(nbuf);
            ++gso_upload_desc_unavailable;
            return -1;
        }

        net_cfg_desc_by_skb(descriptor, skb, nbuf[0x2c] & 1U);
        *(uint32_t *)(void *)descriptor =
            (uint32_t)virt_to_phys(nbuf_data);
        *(uint16_t *)(void *)(descriptor + 4) =
            (uint16_t)((*(uint16_t *)(void *)(descriptor + 4) & 0x8001U) |
                       ((nbuf_length & 0x3fffU) << 1));
        *(uint32_t *)(void *)(descriptor + 16) = 0;
        *(uint32_t *)(void *)(descriptor + 20) = 0;
        descriptor[27] = (descriptor[27] & 0xf3U) | 8U;

        if (net_hw_checksum != 0) {
            descriptor[5] |= 0x80U;
            descriptor[7] &= 0x3fU;
            descriptor[8] = (descriptor[8] & 1U) |
                            ((ip_offset & 0x7fU) << 1);
            descriptor[9] = (descriptor[9] & 1U) |
                            ((tcp_offset & 0x7fU) << 1);
        } else {
            uint8_t *stats = (uint8_t *)cpu_dev_stat(SKB_PTR(skb, 0x10));

            *(uint16_t *)(void *)(descriptor + 22) = nbuf_length;
            if (stats != 0) {
                ++*(uint64_t *)(void *)(stats + 8);
                *(uint64_t *)(void *)(stats + 24) += SKB_U32(skb, 0xa8);
            }
        }

        ++gso_upload_send_count;
        if (cpu_net_nb_desc_tx(nbuf, descriptor) < 0)
            return -1;
        tcp_sequence = next_sequence;
    }

    if (net_gso_debug != 0)
        --net_gso_debug;
    ++gso_normal_success_count;
    if (net_smb_state != 2U)
        net_smb_state = 2;
    return 0;
}

void net_gso_checksum_upload(void *ipv4_header, void *tcp_header,
                             unsigned int upload_template)
{
    uint8_t *ipv4 = ipv4_header;
    uint8_t *tcp = tcp_header;
    unsigned int tcp_length =
        vendor_bswap16(*(uint16_t *)(void *)(ipv4 + 2)) -
        4U * (ipv4[0] & 0x0fU);
    unsigned int checksum_length = tcp_length;
    uint32_t checksum;

    *(uint16_t *)(void *)(tcp + 16) = 0;
    if (upload_template != 0)
        checksum_length = 4U * (tcp[12] >> 4);
    checksum = csum_partial(tcp, checksum_length, 0);
    checksum = csum_tcpudp_nofold(
        *(uint32_t *)(void *)(ipv4 + 12),
        *(uint32_t *)(void *)(ipv4 + 16), tcp_length, 6, checksum);
    *(uint16_t *)(void *)(tcp + 16) = vendor_csum_fold(checksum);
    ip_send_check(ipv4);
}

void net_gso_ipv6tcp_checksum_constprop_6(void *ipv6_header,
                                           void *tcp_header)
{
    uint8_t *ipv6 = ipv6_header;
    uint8_t *tcp = tcp_header;
    unsigned int tcp_header_length = 4U * (tcp[12] >> 4);
    unsigned int ipv6_payload_length =
        vendor_bswap16(*(uint16_t *)(void *)(ipv6 + 4));
    uint32_t partial;

    *(uint16_t *)(void *)(tcp + 16) = 0;
    partial = csum_partial(tcp, tcp_header_length, 0);
    *(uint16_t *)(void *)(tcp + 16) = csum_ipv6_magic(
        ipv6 + 8, ipv6 + 24, ipv6_payload_length, 6, partial);
}

void net_cfg_desc_by_skb(void *descriptor, zte_skb_t *skb,
                          unsigned int direction)
{
    uint8_t *txd = descriptor;

    *(uint32_t *)(void *)(txd + 24) = 0;
    *(uint32_t *)(void *)(txd + 4) = 0;
    *(uint32_t *)(void *)(txd + 8) = 0x00400000U;
    txd[7] = 15;
    if (direction != 0 || lan_up != 0) {
        uint8_t port = SKB_U8(skb, 0x108);

        if ((uint8_t)(port - 16U) > 0x20U)
            ++port;
        txd[10] = (txd[10] & 0xc0U) | (port & 0x3fU);
    }
    txd[27] = (txd[27] & 0xf3U) | 8U;
}

void *net_get_next_txdesc(void *queue_pointer)
{
    zte_tx_queue_t *queue = queue_pointer;
    uint32_t producer;

    if (queue->pending >= g_net_check_threshold) {
        net_check_tx_done_nolock(queue);
        if (queue->pending >= queue->depth) {
            ++net_tx_full;
            return 0;
        }
    }

    producer = queue->producer;
    ++queue->producer;
    if (queue->producer >= queue->depth)
        queue->producer = 0;
    return &queue->descriptor_base[producer];
}

void net_set_prev_txdesc(zte_tx_queue_t *queue)
{
    if (queue->producer == 0)
        queue->producer = queue->depth;
    --queue->producer;
}

int cpu_net_nb_tx(zte_nbuf_t *nbuf)
{
    unsigned int cpu = *(uint32_t *)((uint8_t *)cpu_number +
                                     read_tpidr_el1());
    unsigned int queue_selector = cpu2unlock_tq[cpu];
    zte_tx_queue_t *queue;
    unsigned int queued = 0;
    zte_nbuf_t *last = 0;

    if (queue_selector > 1U) {
        if (__printk_ratelimit("cpu_net_nb_tx"))
            printk("cpu_net_nb_tx cpu %u cpu2unlock_tq[cpu] %u failed\n",
                   cpu, queue_selector);
        return -1;
    }

    queue = unlock_tq[queue_selector];
    while (nbuf != 0) {
        zte_nbuf_t *next = nbuf->next;
        zte_tx_descriptor_t *descriptor;
        uint8_t *stats;

        nbuf->next = 0;
        descriptor = net_get_next_txdesc(queue);
        if (descriptor == 0) {
            cpu_net_free_nbuf(nbuf);
            stats = (uint8_t *)cpu_dev_stat(nbuf->device);
            if (stats != 0)
                ++*(uint64_t *)(void *)(stats + 0x38);
            if (__printk_ratelimit("cpu_net_nb_tx"))
                printk("nb get tx desc failed\n");
        } else {
            uint16_t descriptor_length;
            unsigned int descriptor_index =
                (unsigned int)(((uint8_t *)descriptor -
                                (uint8_t *)queue->descriptor_base) >> 5);

            ++queued;
            ++queue->pending;
            descriptor->data_physical = (uint32_t)virt_to_phys(nbuf->data);
            descriptor->word_4 = nbuf->descriptor_word_4;
            descriptor->word_8 = nbuf->descriptor_word_8;
            descriptor->word_18 = nbuf->descriptor_word_18;

            /* Bits 1 through 14 carry the accumulated 14-bit length. */
            descriptor_length = *(uint16_t *)(void *)&descriptor->word_4;
            descriptor_length = (uint16_t)((descriptor_length & 0x8001U) |
                (((((descriptor_length >> 1) & 0x3fffU) + nbuf->length) &
                  0x3fffU) << 1));
            *(uint16_t *)(void *)&descriptor->word_4 = descriptor_length;
            queue->owners[descriptor_index] = (uintptr_t)nbuf | 1U;

            if (net_tx_debug > 0 &&
                dump_net_check(nbuf->data,
                               (descriptor_length >> 1) & 0x3fffU) == 0) {
                printk("index %u %px\n", descriptor_index,
                       (void *)queue->owners[descriptor_index]);
                printk("send nb %px data %px l %u\n", nbuf, nbuf->data,
                       (descriptor_length >> 1) & 0x3fffU);
                dump_tx_desc(descriptor);
                dump_net_data(nbuf->data,
                              (descriptor_length >> 1) & 0x3fffU);
                --net_tx_debug;
            }

            stats = (uint8_t *)cpu_dev_stat(nbuf->device);
            if (stats != 0) {
                *(uint64_t *)(void *)(stats + 0x18) += nbuf->length;
                if (queued > 0xffU) {
                    stats = (uint8_t *)cpu_dev_stat(nbuf->device);
                    if (stats != 0)
                        *(uint64_t *)(void *)(stats + 8) += queued;
                }
            }
            if (queued > 0xffU) {
                cpu_net_ops->update_tx_queue(queue->hardware_queue, queued);
                if (net_tx_debug > 0)
                    printk("nb tx %u\n", queued);
                queued = 0;
            }
        }

        last = nbuf;
        nbuf = next;
    }

    if (queued != 0) {
        uint8_t *stats = (uint8_t *)cpu_dev_stat(last->device);

        if (stats != 0)
            *(uint64_t *)(void *)(stats + 8) += queued;
        cpu_net_ops->update_tx_queue(queue->hardware_queue, queued);
        if (net_tx_debug > 0 && g_net_dump_select == 0)
            printk("nb tx %u\n", queued);
    }
    return 0;
}

void cpu_lowpower_tx(zte_net_device_t *device, zte_skb_t *skb,
                     zte_tx_descriptor_t *descriptor)
{
    unsigned int type;
    int adjust_length = 0;

    if (low_power_send == 0 || low_power_up_en_judge == 0)
        return;
    if (low_power_up_en_judge() == 0)
        return;

    type = NETDEV_U32(device, 0x888);
    if (type == 3U)
        return;

    if (type != 2U && (g_pon_work_mode & 0xe40U) == 0 &&
        (g_pon_work_mode & 0x1a0U) != 0) {
        low_power_send(0, 0, SKB_PTR(skb, 0x130), SKB_U32(skb, 0xa8), 0);
    }

    if (low_power_up_en_judge() == 2) {
        adjust_length = NETDEV_U32(device, 0x888) != 2U;
    } else if (low_power_up_en_judge() != 2 &&
               (NETDEV_U32(device, 0x888) & ~2U) != 0) {
        adjust_length = NETDEV_U32(device, 0x888) != 2U;
    }

    if (adjust_length != 0 &&
        (((uint8_t *)(void *)&descriptor->word_8)[2] & 0x7fU) == 64U) {
        uint16_t descriptor_length =
            *(uint16_t *)(void *)&descriptor->word_4;

        descriptor_length = (uint16_t)((descriptor_length & 0x8001U) |
            (((SKB_U32(skb, 0xa8) + 4U) & 0x3fffU) << 1));
        *(uint16_t *)(void *)&descriptor->word_4 = descriptor_length;
    }
}

zte_low_power_send_t regisetr_low_power_send_pkt_handle(
    zte_low_power_send_t callback)
{
    low_power_send = callback;
    return callback;
}

zte_low_power_up_en_judge_t regisetr_low_power_up_en_judge_handle(
    zte_low_power_up_en_judge_t callback)
{
    low_power_up_en_judge = callback;
    return callback;
}

zte_omci_oam_rx_t register_omci_oam_handle(zte_omci_oam_rx_t callback)
{
    omci_oam_rx = callback;
    return callback;
}

zte_omci_mic_handler_t regisetr_omci_mic_add_handle(
    zte_omci_mic_handler_t callback)
{
    omci_mic_add = callback;
    return callback;
}

void idm_omci_portid_set(void)
{
}

zte_omci_mic_handler_t register_omci_mic_check_handle(
    zte_omci_mic_handler_t callback)
{
    omci_mic_check = callback;
    return callback;
}

zte_recycle_callback_t register_woe_recycle_handle(
    zte_recycle_callback_t callback)
{
    idm_recycle_cb[0] = callback;
    return callback;
}

zte_recycle_callback_t register_woe1_recycle_handle(
    zte_recycle_callback_t callback)
{
    idm_recycle_cb[1] = callback;
    return callback;
}

zte_recycle_callback_t register_woe2_recycle_handle(
    zte_recycle_callback_t callback)
{
    idm_recycle_cb[2] = callback;
    return callback;
}

void *register_wlan_to_essid_handle(void *callback)
{
    idm_wlanname_to_essid = callback;
    return callback;
}

int cpu_net_nb_desc_tx(void *nbuf, void *descriptor)
{
    uint8_t *queue = cpu_tq;
    uintptr_t descriptor_index =
        ((uintptr_t)descriptor - (uintptr_t)*(void **)(void *)queue) >> 5;
    uintptr_t *owners = *(uintptr_t **)(void *)(queue + 8);

    owners[descriptor_index] = (uintptr_t)nbuf | 1U;
    ++*(uint32_t *)(void *)(queue + 0x14);
    cpu_net_ops->update_tx_queue(
        *(uint32_t *)(void *)(queue + 0x1c), 1);
    return 0;
}

int pp_net_tcp_gro(void *descriptor, zte_net_device_t *device,
                   const void *data, unsigned int queue, unsigned int jumbo)
{
    uint8_t *rxd = descriptor;
    const uint8_t *packet = data;
    unsigned int payload_offset = RXD_U8(rxd, 13);
    const uint8_t *ipv4 = packet + payload_offset;
    const uint8_t *outer_ipv4 = 0;
    unsigned int packet_length = RXD_U16(rxd, 4) & 0x3fffU;
    unsigned int ip_header_length;
    const uint8_t *tcp;
    uint32_t hash;
    zte_gro_flow_t *flow;
    zte_netdev_stats_t *stats;

    if (gro_be16(ipv4 - 2) == 0x0021U)
        outer_ipv4 = ipv4 - 8;

    if (packet_length != payload_offset + gro_be16(ipv4 + 2)) {
        ++net_gro_bad_length;
        return 0;
    }

    ip_header_length = 4U * (ipv4[0] & 0x0fU);
    tcp = ipv4 + ip_header_length;
    hash = gro_flow_hash(ipv4, tcp);
    flow = gro_find_exact_flow(hash, ipv4, tcp, rxd);

    if (gro_be16(tcp + 2) != 445U &&
        !is_l4port_supported(gro_be16(tcp + 2), 1) &&
        !is_l4port_supported(gro_be16(tcp), 0)) {
        ++net_gro_unsupported_port;
        return 0;
    }

    if (!can_tcp_gro(flow != 0 ? flow->skb : 0, ipv4, tcp, rxd)) {
        unsigned int bucket;

        for (bucket = 0; bucket < 16; ++bucket) {
            zte_gro_hlist_node_t *node = gro_hash_table[bucket];

            while (node != 0) {
                zte_gro_hlist_node_t *next = node->next;
                zte_gro_flow_t *old_flow = gro_flow_from_node(node);

                pp_tcp_gro_flush(old_flow->skb);
                hlist_del_init(&old_flow->node);
                --g_cur_flows;
                kfree(old_flow);
                node = next;
            }
        }
        return 0;
    }

    stats = cpu_dev_stat(device);
    if (stats != 0) {
        ++stats->rx_packets;
        stats->rx_bytes += packet_length;
    }

    /* The binary branches on the hash-only scan, then dereferences exact flow. */
    if (search_gro_flow(hash)) {
        zte_skb_t *skb = flow->skb;
        uint8_t *shared = (uint8_t *)SKB_PTR(skb, 0x128) + SKB_U32(skb, 0x120);
        const uint8_t *payload = tcp + 4U * ((tcp[12] >> 4) & 0x0fU);
        unsigned int payload_length = (unsigned int)((packet + packet_length) -
                                                      payload);
        unsigned int fragment = shared[2];
        uint8_t *frag = shared + 0x30U + 16U * fragment;
        uint32_t *buffer_pointers = SKB_PTR(skb, 0x50);

        *(uint64_t *)(void *)frag = gro_encode_fragment_page(payload);
        *(uint32_t *)(void *)(frag + 8) = payload_length;
        *(uint32_t *)(void *)(frag + 12) = (uint16_t)(uintptr_t)payload & 0xfffU;
        buffer_pointers[fragment] = RXD_U32(rxd, 0);
        shared[2] = (uint8_t)(fragment + 1U);
        SKB_U32(skb, 0xa8) += payload_length;
        SKB_U32(skb, 0xac) += payload_length;
        SKB_U32(skb, 0x138) += payload_length;

        if (net_gro_debug != 0)
            --net_gro_debug;
        if (g_cur_flows > 1)
            ++net_gro_multi_flow_count;

        if (payload_length <= 0x54fU || shared[2] >= max_gro) {
            pp_tcp_gro_flush(skb);
            hlist_del_init(&flow->node);
            --g_cur_flows;
            kfree(flow);
        }
        return 1;
    }

    if (g_cur_flows == 16)
        return 0;

    flow = kmem_cache_alloc(gro_state_cache, 0xb20U);
    if (flow == 0) {
        ++net_gro_alloc_failure;
        return 0;
    }

    flow->skb = alloc_skb_attach_buffer(
        0, (void *)(packet - (uBP_BUFFER_OFFSET + 64U)),
        uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE, uBP_BUFFER_OFFSET + 64U,
        RXD_U32(rxd, 0));
    if (flow->skb == 0) {
        kfree(flow);
        ++net_gro_alloc_failure;
        return 0;
    }

    flow->hash = hash;
    gro_add_flow(flow, gro_bucket(hash));
    ++g_cur_flows;
    wifi_gro_rxq = queue;
    for (unsigned int i = 0; i < 8; ++i)
        wifi_gro_desc[i] = ((const uint32_t *)(const void *)rxd)[i];

    SKB_U32(flow->skb, 0x114) |= 0x8008U;
    SKB_U8(flow->skb, 0xb8) = (SKB_U8(flow->skb, 0xb8) & 0x9fU) | 0x20U;
    SKB_U16(flow->skb, 0x10e) =
        (uint16_t)(uBP_BUFFER_OFFSET + packet_length + 64U);
    SKB_PTR(flow->skb, 0x10) = device;
    if (jumbo == 0)
        SKB_U8(flow->skb, 0x108) = (RXD_U8(rxd, 6) & 0x3fU) - 1U;
    skb_put(flow->skb, packet_length);

    if ((((RXD_U8(rxd, 6) & 0x3fU) + 0x30U) & 0x3fU) > 0x20U)
        SKB_U8(flow->skb, 0x108) = (RXD_U8(rxd, 6) & 0x3fU) - 1U;
    else
        SKB_U8(flow->skb, 0x108) = RXD_U8(rxd, 6) & 0x3fU;
    if (outer_ipv4 != 0)
        SKB_PTR(flow->skb, 0x40) = (void *)outer_ipv4;
    SKB_PTR(flow->skb, 0x30) = (void *)ipv4;
    SKB_PTR(flow->skb, 0x38) = (void *)tcp;
    SKB_U8(flow->skb, 0x58) = RXD_U8(rxd, 6) & 0x3fU;
    SKB_U32(flow->skb, 0x48) = (uint32_t)jiffies;
    SKB_PTR(flow->skb, 0x50) =
        (void *)(packet + uNORMAL_BP_SIZE - uSKB_SHAREDINFO_SIZE - 68U -
                 (uBP_BUFFER_OFFSET + 64U));
    ++net_gro_new_flow_count;
    return 1;
}

int net_tst_tx(const void *data, unsigned int length, unsigned int port)
{
    zte_net_device_t *device;
    zte_skb_t *skb;
    zte_netdev_stats_t *stats;

    if (length == 0U || port > 3U || data == 0)
        return -1;

    device = cpu_netdev_slots[port];
    skb = __netdev_alloc_skb(device, length + 16U, NET_TST_TX_ALLOC_FLAGS);
    if (skb == 0) {
        stats = cpu_dev_stat(device);
        if (stats != 0)
            ++stats->tx_dropped;
        return 0;
    }

    skb_put(skb, length);
    memcpy(SKB_PTR(skb, 0x130), data, length);
    SKB_PTR(skb, 0x10) = device;
    cpu_net_tx(skb, device);
    return 0;
}

int oam_tx(const void *data, unsigned int length)
{
    return net_tst_tx(data, length, 2U);
}

void net_omci_tx_test(unsigned int length)
{
    uint8_t *data;
    unsigned int index;

    data = __kmalloc(length + 10U, NET_TST_TX_ALLOC_FLAGS);
    if (data == 0) {
        printk("alloc data error\n");
        return;
    }

    printk("send omci/oam data:");
    for (index = 0; index != length; ++index) {
        data[index] = (uint8_t)index;
        printk("%02x ", (uint8_t)index);
    }
    printk("\n");
    net_tst_tx(data, length, 2U);
    kfree(data);
}
