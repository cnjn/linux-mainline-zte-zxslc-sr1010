// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE

#include <errno.h>
#include <linux/bpf.h>
#include <linux/ethtool.h>
#include <linux/if_link.h>
#include <linux/if_xdp.h>
#include <linux/if_ether.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define FRAME_SIZE	2048U
#define FRAME_COUNT	4096U
#define RX_RING_SIZE	1024U
#define TX_RING_SIZE	1024U
#define FILL_RING_SIZE	FRAME_COUNT
#define COMP_RING_SIZE	FRAME_COUNT

struct xsk_ring {
	uint32_t *producer;
	uint32_t *consumer;
	uint32_t *flags;
	void *desc;
	uint32_t mask;
};

struct xsk_state {
	int fd;
	void *umem;
	struct xsk_ring rx;
	struct xsk_ring tx;
	struct xsk_ring fill;
	struct xsk_ring comp;
	uint32_t rx_cons;
	uint32_t tx_prod;
	uint32_t fill_prod;
	uint32_t comp_cons;
};

static volatile sig_atomic_t stop;

static void stop_handler(int signo)
{
	(void)signo;
	stop = 1;
}

static uint32_t load_acquire(const uint32_t *value)
{
	return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static void store_release(uint32_t *value, uint32_t update)
{
	__atomic_store_n(value, update, __ATOMIC_RELEASE);
}

static int bpf_cmd(enum bpf_cmd cmd, union bpf_attr *attr)
{
	return syscall(__NR_bpf, cmd, attr, sizeof(*attr));
}

#define BPF_RAW_INSN(CODE, DST, SRC, OFF, IMM) \
	((struct bpf_insn){ .code = CODE, .dst_reg = DST, .src_reg = SRC, \
			    .off = OFF, .imm = IMM })
#define BPF_MOV64_IMM(DST, IMM) \
	BPF_RAW_INSN(BPF_ALU64 | BPF_MOV | BPF_K, DST, 0, 0, IMM)
#define BPF_EMIT_CALL(FUNC) \
	BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, FUNC)
#define BPF_EXIT_INSN() BPF_RAW_INSN(BPF_JMP | BPF_EXIT, 0, 0, 0, 0)

static int create_xsk_map(void)
{
	union bpf_attr attr = {};

	attr.map_type = BPF_MAP_TYPE_XSKMAP;
	attr.key_size = sizeof(uint32_t);
	attr.value_size = sizeof(uint32_t);
	attr.max_entries = 1;
	memcpy(attr.map_name, "zx_xsk", sizeof("zx_xsk"));
	return bpf_cmd(BPF_MAP_CREATE, &attr);
}

static int load_redirect_prog(int map_fd)
{
	char log[16384] = {};
	char license[] = "GPL";
	struct bpf_insn insns[] = {
		BPF_RAW_INSN(BPF_LD | BPF_DW | BPF_IMM, BPF_REG_1,
			     BPF_PSEUDO_MAP_FD, 0, map_fd),
		BPF_RAW_INSN(0, 0, 0, 0, 0),
		BPF_MOV64_IMM(BPF_REG_2, 0),
		BPF_MOV64_IMM(BPF_REG_3, XDP_PASS),
		BPF_EMIT_CALL(BPF_FUNC_redirect_map),
		BPF_EXIT_INSN(),
	};
	union bpf_attr attr = {};
	int fd;

	attr.prog_type = BPF_PROG_TYPE_XDP;
	attr.expected_attach_type = BPF_XDP;
	attr.insn_cnt = sizeof(insns) / sizeof(insns[0]);
	attr.insns = (uintptr_t)insns;
	attr.license = (uintptr_t)license;
	attr.log_buf = (uintptr_t)log;
	attr.log_size = sizeof(log);
	attr.log_level = 1;
	memcpy(attr.prog_name, "zx_xsk_redirect", sizeof("zx_xsk_redirect"));
	fd = bpf_cmd(BPF_PROG_LOAD, &attr);
	if (fd < 0)
		fprintf(stderr, "BPF_PROG_LOAD: %s\n%s\n", strerror(errno), log);
	return fd;
}

static int update_xsk_map(int map_fd, int xsk_fd)
{
	uint32_t key = 0;
	uint32_t value = xsk_fd;
	union bpf_attr attr = {};

	attr.map_fd = map_fd;
	attr.key = (uintptr_t)&key;
	attr.value = (uintptr_t)&value;
	attr.flags = BPF_ANY;
	return bpf_cmd(BPF_MAP_UPDATE_ELEM, &attr);
}

static int attach_xdp_link(int prog_fd, unsigned int ifindex)
{
	union bpf_attr attr = {};

	attr.link_create.prog_fd = prog_fd;
	attr.link_create.target_ifindex = ifindex;
	attr.link_create.attach_type = BPF_XDP;
	attr.link_create.flags = XDP_FLAGS_DRV_MODE;
	return bpf_cmd(BPF_LINK_CREATE, &attr);
}

static size_t ring_map_size(const struct xdp_ring_offset *offset,
			    uint32_t count, size_t desc_size)
{
	size_t desc_end = offset->desc + count * desc_size;
	size_t flags_end = offset->flags + sizeof(uint32_t);
	size_t size = desc_end > flags_end ? desc_end : flags_end;
	long page_size = sysconf(_SC_PAGESIZE);

	return (size + page_size - 1) & ~(page_size - 1);
}

static int map_ring(int fd, struct xsk_ring *ring,
		    const struct xdp_ring_offset *offset, uint32_t count,
		    size_t desc_size, off_t mmap_offset)
{
	void *map;

	map = mmap(NULL, ring_map_size(offset, count, desc_size),
		   PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
		   fd, mmap_offset);
	if (map == MAP_FAILED)
		return -1;
	ring->producer = map + offset->producer;
	ring->consumer = map + offset->consumer;
	ring->flags = map + offset->flags;
	ring->desc = map + offset->desc;
	ring->mask = count - 1;
	return 0;
}

static int setup_xsk(struct xsk_state *xsk, unsigned int ifindex)
{
	struct xdp_mmap_offsets offsets;
	struct xdp_umem_reg umem = {};
	struct sockaddr_xdp address = {};
	struct xdp_options options = {};
	socklen_t option_len;
	uint32_t ring_size;
	size_t umem_size = (size_t)FRAME_SIZE * FRAME_COUNT;
	uint64_t *fill_desc;
	unsigned int i;

	xsk->fd = socket(AF_XDP, SOCK_RAW, 0);
	if (xsk->fd < 0)
		return -1;
	xsk->umem = mmap(NULL, umem_size, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
	if (xsk->umem == MAP_FAILED)
		return -1;

	umem.addr = (uintptr_t)xsk->umem;
	umem.len = umem_size;
	umem.chunk_size = FRAME_SIZE;
	if (setsockopt(xsk->fd, SOL_XDP, XDP_UMEM_REG,
		       &umem, sizeof(umem)))
		return -1;

	ring_size = FILL_RING_SIZE;
	if (setsockopt(xsk->fd, SOL_XDP, XDP_UMEM_FILL_RING,
		       &ring_size, sizeof(ring_size)))
		return -1;
	ring_size = COMP_RING_SIZE;
	if (setsockopt(xsk->fd, SOL_XDP, XDP_UMEM_COMPLETION_RING,
		       &ring_size, sizeof(ring_size)))
		return -1;
	ring_size = RX_RING_SIZE;
	if (setsockopt(xsk->fd, SOL_XDP, XDP_RX_RING,
		       &ring_size, sizeof(ring_size)))
		return -1;
	ring_size = TX_RING_SIZE;
	if (setsockopt(xsk->fd, SOL_XDP, XDP_TX_RING,
		       &ring_size, sizeof(ring_size)))
		return -1;

	option_len = sizeof(offsets);
	if (getsockopt(xsk->fd, SOL_XDP, XDP_MMAP_OFFSETS,
		       &offsets, &option_len))
		return -1;
	if (map_ring(xsk->fd, &xsk->rx, &offsets.rx, RX_RING_SIZE,
		     sizeof(struct xdp_desc), XDP_PGOFF_RX_RING) ||
	    map_ring(xsk->fd, &xsk->tx, &offsets.tx, TX_RING_SIZE,
		     sizeof(struct xdp_desc), XDP_PGOFF_TX_RING) ||
	    map_ring(xsk->fd, &xsk->fill, &offsets.fr, FILL_RING_SIZE,
		     sizeof(uint64_t), XDP_UMEM_PGOFF_FILL_RING) ||
	    map_ring(xsk->fd, &xsk->comp, &offsets.cr, COMP_RING_SIZE,
		     sizeof(uint64_t), XDP_UMEM_PGOFF_COMPLETION_RING))
		return -1;

	fill_desc = xsk->fill.desc;
	for (i = 0; i < FRAME_COUNT; i++)
		fill_desc[i] = (uint64_t)i * FRAME_SIZE;
	xsk->fill_prod = FRAME_COUNT;
	store_release(xsk->fill.producer, xsk->fill_prod);

	address.sxdp_family = AF_XDP;
	address.sxdp_ifindex = ifindex;
	address.sxdp_queue_id = 0;
	address.sxdp_flags = XDP_ZEROCOPY | XDP_USE_NEED_WAKEUP;
	if (bind(xsk->fd, (struct sockaddr *)&address, sizeof(address)))
		return -1;

	option_len = sizeof(options);
	if (getsockopt(xsk->fd, SOL_XDP, XDP_OPTIONS,
		       &options, &option_len))
		return -1;
	if (!(options.flags & XDP_OPTIONS_ZEROCOPY)) {
		errno = EOPNOTSUPP;
		return -1;
	}

	return 0;
}

static unsigned int recycle_completions(struct xsk_state *xsk)
{
	uint64_t *comp_desc = xsk->comp.desc;
	uint64_t *fill_desc = xsk->fill.desc;
	uint32_t comp_prod = load_acquire(xsk->comp.producer);
	uint32_t fill_cons = load_acquire(xsk->fill.consumer);
	unsigned int recycled = 0;

	while (xsk->comp_cons != comp_prod &&
	       xsk->fill_prod - fill_cons < FILL_RING_SIZE) {
		uint64_t address = comp_desc[xsk->comp_cons & xsk->comp.mask];

		fill_desc[xsk->fill_prod & xsk->fill.mask] = address;
		xsk->comp_cons++;
		xsk->fill_prod++;
		recycled++;
	}
	if (recycled) {
		store_release(xsk->comp.consumer, xsk->comp_cons);
		store_release(xsk->fill.producer, xsk->fill_prod);
	}
	return recycled;
}

static unsigned int echo_rx(struct xsk_state *xsk, uint64_t *bytes)
{
	struct xdp_desc *rx_desc = xsk->rx.desc;
	struct xdp_desc *tx_desc = xsk->tx.desc;
	uint32_t rx_prod = load_acquire(xsk->rx.producer);
	uint32_t tx_cons = load_acquire(xsk->tx.consumer);
	unsigned int packets = 0;

	while (xsk->rx_cons != rx_prod &&
	       xsk->tx_prod - tx_cons < TX_RING_SIZE) {
		struct xdp_desc packet = rx_desc[xsk->rx_cons & xsk->rx.mask];
		uint64_t offset = packet.addr & (FRAME_SIZE - 1);
		uint64_t frame = packet.addr & ~(uint64_t)(FRAME_SIZE - 1);
		uint8_t *data = xsk->umem + frame + offset;
		unsigned int i;

		if (packet.len >= 2 * ETH_ALEN) {
			for (i = 0; i < ETH_ALEN; i++) {
				uint8_t tmp = data[i];

				data[i] = data[ETH_ALEN + i];
				data[ETH_ALEN + i] = tmp;
			}
		}
		packet.options = 0;
		tx_desc[xsk->tx_prod & xsk->tx.mask] = packet;
		xsk->rx_cons++;
		xsk->tx_prod++;
		packets++;
		*bytes += packet.len;
	}
	if (packets) {
		store_release(xsk->rx.consumer, xsk->rx_cons);
		store_release(xsk->tx.producer, xsk->tx_prod);
	}
	return packets;
}

static void wake_driver(struct xsk_state *xsk)
{
	if ((*xsk->tx.flags & XDP_RING_NEED_WAKEUP) ||
	    (*xsk->fill.flags & XDP_RING_NEED_WAKEUP))
		sendto(xsk->fd, NULL, 0, MSG_DONTWAIT, NULL, 0);
}

static void print_driver_stats(const char *ifname)
{
	struct ethtool_drvinfo info = { .cmd = ETHTOOL_GDRVINFO };
	struct ethtool_gstrings *names = NULL;
	struct ethtool_stats *stats = NULL;
	struct ifreq ifr = {};
	unsigned int i;
	int fd;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return;
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	ifr.ifr_data = (void *)&info;
	if (ioctl(fd, SIOCETHTOOL, &ifr) || !info.n_stats)
		goto out;
	names = calloc(1, sizeof(*names) + info.n_stats * ETH_GSTRING_LEN);
	stats = calloc(1, sizeof(*stats) + info.n_stats * sizeof(uint64_t));
	if (!names || !stats)
		goto out;
	names->cmd = ETHTOOL_GSTRINGS;
	names->string_set = ETH_SS_STATS;
	names->len = info.n_stats;
	ifr.ifr_data = (void *)names;
	if (ioctl(fd, SIOCETHTOOL, &ifr))
		goto out;
	stats->cmd = ETHTOOL_GSTATS;
	stats->n_stats = info.n_stats;
	ifr.ifr_data = (void *)stats;
	if (ioctl(fd, SIOCETHTOOL, &ifr))
		goto out;
	for (i = 0; i < info.n_stats; i++) {
		char name[ETH_GSTRING_LEN + 1] = {};

		memcpy(name, names->data + i * ETH_GSTRING_LEN,
		       ETH_GSTRING_LEN);
		if (!strncmp(name, "xdp_", 4) ||
		    !strncmp(name, "rx_page_map", 11) ||
		    !strncmp(name, "rx_refill", 9))
			printf("driver.%s=%llu\n", name,
			       (unsigned long long)stats->data[i]);
	}

out:
	free(stats);
	free(names);
	close(fd);
}

int main(int argc, char **argv)
{
	struct rlimit memlock = { RLIM_INFINITY, RLIM_INFINITY };
	struct xdp_statistics stats = {};
	struct xsk_state xsk = { .fd = -1 };
	struct pollfd pollfd = {};
	unsigned int ifindex;
	unsigned int seconds = 30;
	uint64_t completions = 0;
	uint64_t rx_bytes = 0;
	uint64_t rx_packets = 0;
	time_t started;
	int map_fd = -1;
	int prog_fd = -1;
	int link_fd = -1;
	int rc = EXIT_FAILURE;

	if (argc < 2 || argc > 3) {
		fprintf(stderr, "usage: %s IFACE [SECONDS]\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (argc == 3)
		seconds = strtoul(argv[2], NULL, 0);
	ifindex = if_nametoindex(argv[1]);
	if (!ifindex) {
		perror("if_nametoindex");
		return EXIT_FAILURE;
	}
	setrlimit(RLIMIT_MEMLOCK, &memlock);
	signal(SIGINT, stop_handler);
	signal(SIGTERM, stop_handler);

	if (setup_xsk(&xsk, ifindex)) {
		perror("AF_XDP zero-copy setup");
		goto out;
	}
	map_fd = create_xsk_map();
	if (map_fd < 0) {
		perror("BPF_MAP_CREATE");
		goto out;
	}
	prog_fd = load_redirect_prog(map_fd);
	if (prog_fd < 0)
		goto out;
	if (update_xsk_map(map_fd, xsk.fd)) {
		perror("BPF_MAP_UPDATE_ELEM");
		goto out;
	}
	link_fd = attach_xdp_link(prog_fd, ifindex);
	if (link_fd < 0) {
		perror("BPF_LINK_CREATE");
		goto out;
	}

	printf("AF_XDP zero-copy active: if=%s queue=0 frames=%u frame_size=%u\n",
	       argv[1], FRAME_COUNT, FRAME_SIZE);
	pollfd.fd = xsk.fd;
	pollfd.events = POLLIN;
	started = time(NULL);
	while (!stop && (!seconds || time(NULL) - started < seconds)) {
		unsigned int done;

		done = recycle_completions(&xsk);
		completions += done;
		rx_packets += echo_rx(&xsk, &rx_bytes);
		wake_driver(&xsk);
		if (!done && xsk.rx_cons == load_acquire(xsk.rx.producer))
			poll(&pollfd, 1, 100);
	}
	completions += recycle_completions(&xsk);
	{
		socklen_t len = sizeof(stats);

		getsockopt(xsk.fd, SOL_XDP, XDP_STATISTICS, &stats, &len);
	}
	printf("rx=%llu tx=%llu completions=%llu bytes=%llu "
	       "rx_dropped=%llu rx_invalid=%llu tx_invalid=%llu "
	       "rx_ring_full=%llu fill_empty=%llu tx_empty=%llu\n",
	       (unsigned long long)rx_packets,
	       (unsigned long long)rx_packets,
	       (unsigned long long)completions,
	       (unsigned long long)rx_bytes,
	       (unsigned long long)stats.rx_dropped,
	       (unsigned long long)stats.rx_invalid_descs,
	       (unsigned long long)stats.tx_invalid_descs,
	       (unsigned long long)stats.rx_ring_full,
	       (unsigned long long)stats.rx_fill_ring_empty_descs,
	       (unsigned long long)stats.tx_ring_empty_descs);
	print_driver_stats(argv[1]);
	rc = rx_packets && completions ? EXIT_SUCCESS : EXIT_FAILURE;

out:
	if (link_fd >= 0)
		close(link_fd);
	if (prog_fd >= 0)
		close(prog_fd);
	if (map_fd >= 0)
		close(map_fd);
	if (xsk.fd >= 0)
		close(xsk.fd);
	return rc;
}
