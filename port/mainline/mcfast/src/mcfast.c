// SPDX-License-Identifier: GPL-2.0-only
/*
 * mcfast: keep the tc BPF multicast fast path in sync with the kernel's
 * IPv4 multicast routing table (MFC).
 *
 * Only resolved routes are offered to the fast path, so the multicast
 * router daemon (igmpproxy, smcroute, ...) and the firewall still decide
 * what gets forwarded. A route stays in the kernel when
 *  - its ingress device is neither Ethernet nor a raw-IP device (PPP),
 *  - one of its egress devices is not Ethernet (a bridge, a port, ...),
 *  - it has no or too many egress devices, or
 *  - something on the router itself listens to the group on the ingress
 *    device besides the routing daemon's own membership (e.g. udpxy).
 */
#include <errno.h>
#include <getopt.h>
#include <net/if.h>
#include <poll.h>
#include <stddef.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if_arp.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/types.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "mcfast.h"

#define MCFAST_OBJ		"/lib/bpf/mcfast-bpf.o"
#define MCFAST_STATUS		"/var/run/mcfast.status"
#define MCFAST_MAX_MFC		MCFAST_MAX_ROUTES
#define MCFAST_MAX_IIFS		32
#define MCFAST_MAX_VIFS		32
/* Memberships the routing daemon itself holds on its upstream device. */
#define MCFAST_DAEMON_USERS	1

struct mfc {
	struct mcfast_key key;
	int n_oifs;
	__u32 oif[MCFAST_MAX_VIFS];
	__u8 ttl[MCFAST_MAX_VIFS];
};

struct want {
	struct mcfast_key key;
	struct mcfast_route route;
	int iif_type;
};

struct iif_link {
	__u32 ifindex;
	int type;
	struct bpf_link *link;
};

static struct bpf_object *obj;
static struct bpf_program *prog_eth, *prog_ip;
static int map_fd;
static struct iif_link links[MCFAST_MAX_IIFS];
static int n_links;
static volatile sig_atomic_t stop;
static int verbose;
static __u32 nl_seq;

static struct mfc mfcs[MCFAST_MAX_MFC];
static struct want wants[MCFAST_MAX_MFC];

#define dbg(...) do { if (verbose) fprintf(stderr, __VA_ARGS__); } while (0)

static void on_signal(int sig)
{
	stop = 1;
}

static int read_sysfs(const char *ifname, const char *attr, char *buf,
		      size_t len)
{
	char path[64 + IF_NAMESIZE];
	FILE *f;
	bool ok;

	snprintf(path, sizeof(path), "/sys/class/net/%s/%s", ifname, attr);
	f = fopen(path, "r");
	if (!f)
		return -1;
	ok = fgets(buf, len, f);
	fclose(f);
	if (!ok)
		return -1;
	buf[strcspn(buf, "\n")] = 0;
	return 0;
}

/* ARPHRD_* type, MAC address and MTU of an interface. */
static int if_info(__u32 ifindex, int *type, __u8 *mac, unsigned int *mtu)
{
	char name[IF_NAMESIZE], buf[64];
	unsigned int m[6];

	if (!if_indextoname(ifindex, name) ||
	    read_sysfs(name, "type", buf, sizeof(buf)))
		return -1;
	*type = atoi(buf);
	if (read_sysfs(name, "mtu", buf, sizeof(buf)))
		return -1;
	*mtu = atoi(buf);
	if (!mac)
		return 0;
	if (read_sysfs(name, "address", buf, sizeof(buf)) ||
	    sscanf(buf, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3],
		   &m[4], &m[5]) != 6)
		return -1;
	for (int i = 0; i < 6; i++)
		mac[i] = m[i];
	return 0;
}

static int nl_open(__u32 groups)
{
	struct sockaddr_nl sa = {
		.nl_family = AF_NETLINK,
		.nl_groups = groups,
	};
	int fd;

	fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
	if (fd < 0)
		return -1;
	if (bind(fd, (struct sockaddr *)&sa, sizeof(sa))) {
		close(fd);
		return -1;
	}
	return fd;
}

static void parse_mfc(struct nlmsghdr *nh, int *n)
{
	struct rtmsg *rtm = NLMSG_DATA(nh);
	int len = RTM_PAYLOAD(nh);
	struct mfc *m = &mfcs[*n];
	bool have_iif = false;
	struct rtattr *rta;

	if (*n >= MCFAST_MAX_MFC || rtm->rtm_family != RTNL_FAMILY_IPMR ||
	    (rtm->rtm_flags & RTNH_F_UNRESOLVED))
		return;
	memset(m, 0, sizeof(*m));
	for (rta = RTM_RTA(rtm); RTA_OK(rta, len); rta = RTA_NEXT(rta, len)) {
		switch (rta->rta_type) {
		case RTA_SRC:
			memcpy(&m->key.src, RTA_DATA(rta), 4);
			break;
		case RTA_DST:
			memcpy(&m->key.grp, RTA_DATA(rta), 4);
			break;
		case RTA_IIF:
			memcpy(&m->key.ifindex, RTA_DATA(rta), 4);
			have_iif = true;
			break;
		case RTA_MULTIPATH: {
			struct rtnexthop *nh = RTA_DATA(rta);
			int nhlen = RTA_PAYLOAD(rta);

			while (RTNH_OK(nh, nhlen) &&
			       m->n_oifs < MCFAST_MAX_VIFS) {
				m->oif[m->n_oifs] = nh->rtnh_ifindex;
				m->ttl[m->n_oifs] = nh->rtnh_hops;
				m->n_oifs++;
				nhlen -= NLMSG_ALIGN(nh->rtnh_len);
				nh = RTNH_NEXT(nh);
			}
			break;
		}
		}
	}
	if (have_iif && m->key.grp)
		(*n)++;
}

/* Dump the resolved IPv4 multicast routes of all tables into mfcs[]. */
static int dump_mfc(int fd)
{
	struct {
		struct nlmsghdr nh;
		struct rtmsg rtm;
	} req = {
		.nh = {
			.nlmsg_len = sizeof(req),
			.nlmsg_type = RTM_GETROUTE,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
			.nlmsg_seq = ++nl_seq,
		},
		.rtm.rtm_family = RTNL_FAMILY_IPMR,
	};
	static char buf[32768];
	int n = 0;

	if (send(fd, &req, sizeof(req), 0) < 0)
		return -errno;
	for (;;) {
		ssize_t len = recv(fd, buf, sizeof(buf), 0);
		struct nlmsghdr *nh;

		if (len < 0) {
			if (errno == EINTR)
				continue;
			return -errno;
		}
		for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, len);
		     nh = NLMSG_NEXT(nh, len)) {
			if (nh->nlmsg_seq != nl_seq)
				continue;
			if (nh->nlmsg_type == NLMSG_DONE)
				return n;
			if (nh->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *e = NLMSG_DATA(nh);

				return e->error;
			}
			if (nh->nlmsg_type == RTM_NEWROUTE)
				parse_mfc(nh, &n);
		}
	}
}

/*
 * True when sockets on the router other than the routing daemon's joined
 * @grp on @ifindex: the kernel then delivers the stream locally as well,
 * which the fast path would bypass. /proc/net/igmp prints each group as the
 * raw __be32 read as a host-order u32.
 */
static bool local_listeners(__u32 ifindex, __be32 grp)
{
	unsigned int idx = 0, group, users;
	char line[256];
	bool found = false;
	FILE *f;

	f = fopen("/proc/net/igmp", "r");
	if (!f)
		return false;
	while (!found && fgets(line, sizeof(line), f)) {
		if (line[0] >= '0' && line[0] <= '9')
			idx = strtoul(line, NULL, 10);
		else if (line[0] == '\t' && idx == ifindex &&
			 sscanf(line, " %x %u", &group, &users) == 2 &&
			 (__be32)group == grp)
			found = users > MCFAST_DAEMON_USERS;
	}
	fclose(f);
	return found;
}

static bool build_want(const struct mfc *m, struct want *w)
{
	unsigned int mtu, min_mtu = 0xffff;
	int type, n = 0;

	memset(w, 0, sizeof(*w));
	w->key = m->key;
	if (m->n_oifs == 0 || m->n_oifs > MCFAST_MAX_OIFS ||
	    if_info(m->key.ifindex, &w->iif_type, NULL, &mtu) ||
	    (w->iif_type != ARPHRD_ETHER && w->iif_type != ARPHRD_PPP))
		return false;

	for (int i = 0; i < m->n_oifs; i++) {
		struct mcfast_oif *oif = &w->route.oif[n++];

		if (m->oif[i] == m->key.ifindex ||
		    if_info(m->oif[i], &type, oif->mac, &mtu) ||
		    type != ARPHRD_ETHER)
			return false;
		oif->ifindex = m->oif[i];
		if (mtu < min_mtu)
			min_mtu = mtu;
		if (m->ttl[i] > w->route.ttl_min)
			w->route.ttl_min = m->ttl[i];
	}
	w->route.n_oifs = n;
	w->route.mtu = min_mtu;

	return !local_listeners(m->key.ifindex, m->key.grp);
}

static struct iif_link *find_link(__u32 ifindex)
{
	for (int i = 0; i < n_links; i++)
		if (links[i].ifindex == ifindex)
			return &links[i];
	return NULL;
}

/* Attach to the ingress devices in use, detach from the others. */
static void sync_links(int n_want)
{
	bool used[MCFAST_MAX_IIFS] = {};

	for (int i = 0; i < n_want; i++) {
		struct iif_link *l = find_link(wants[i].key.ifindex);
		LIBBPF_OPTS(bpf_tcx_opts, opts, .flags = BPF_F_BEFORE);
		struct bpf_program *prog;

		if (l && l->type != wants[i].iif_type) {
			bpf_link__destroy(l->link);
			*l = links[--n_links];
			used[l - links] = used[n_links];
			l = NULL;
		}
		if (l) {
			used[l - links] = true;
			continue;
		}
		if (n_links == MCFAST_MAX_IIFS)
			continue;
		prog = wants[i].iif_type == ARPHRD_PPP ? prog_ip : prog_eth;
		l = &links[n_links];
		/* A failed attach is kept (without a link) so it is not
		 * retried every round; it is retried once the device stops
		 * being used and comes back.
		 */
		l->link = bpf_program__attach_tcx(prog, wants[i].key.ifindex,
						  &opts);
		if (!l->link)
			fprintf(stderr, "mcfast: attach to ifindex %u: %s\n",
				wants[i].key.ifindex, strerror(errno));
		l->ifindex = wants[i].key.ifindex;
		l->type = wants[i].iif_type;
		used[n_links++] = true;
		dbg("mcfast: attached to ifindex %u\n", l->ifindex);
	}

	for (int i = n_links - 1; i >= 0; i--) {
		if (used[i])
			continue;
		dbg("mcfast: detached from ifindex %u\n", links[i].ifindex);
		bpf_link__destroy(links[i].link);
		links[i] = links[--n_links];
		used[i] = used[n_links];
	}
}

static bool wanted(const struct mcfast_key *key, int n_want)
{
	for (int i = 0; i < n_want; i++)
		if (!memcmp(&wants[i].key, key, sizeof(*key)))
			return true;
	return false;
}

static void sync_map(int n_want)
{
	static struct mcfast_key stale[MCFAST_MAX_ROUTES];
	struct mcfast_key key, *prev = NULL;
	int n_stale = 0;

	while (n_stale < MCFAST_MAX_ROUTES &&
	       !bpf_map_get_next_key(map_fd, prev, &key)) {
		if (!wanted(&key, n_want))
			stale[n_stale++] = key;
		prev = &key;
	}
	for (int i = 0; i < n_stale; i++)
		bpf_map_delete_elem(map_fd, &stale[i]);

	for (int i = 0; i < n_want; i++) {
		struct mcfast_route cur;

		if (!bpf_map_lookup_elem(map_fd, &wants[i].key, &cur)) {
			if (!memcmp(&cur, &wants[i].route,
				    offsetof(struct mcfast_route, packets)))
				continue;
			wants[i].route.packets = cur.packets;
			wants[i].route.bytes = cur.bytes;
		}
		if (bpf_map_update_elem(map_fd, &wants[i].key,
					&wants[i].route, BPF_ANY))
			fprintf(stderr, "mcfast: map update: %s\n",
				strerror(errno));
	}
}

static void write_status(void)
{
	struct mcfast_key key, *prev = NULL;
	struct mcfast_route r;
	FILE *f;

	f = fopen(MCFAST_STATUS ".tmp", "w");
	if (!f)
		return;
	while (!bpf_map_get_next_key(map_fd, prev, &key)) {
		char iif[IF_NAMESIZE] = "?", src[16], grp[16];

		prev = &key;
		if (bpf_map_lookup_elem(map_fd, &key, &r))
			continue;
		if_indextoname(key.ifindex, iif);
		inet_ntop(AF_INET, &key.src, src, sizeof(src));
		inet_ntop(AF_INET, &key.grp, grp, sizeof(grp));
		fprintf(f, "%s %s %s ->", iif, src, grp);
		for (int i = 0; i < r.n_oifs && i < MCFAST_MAX_OIFS; i++) {
			char oif[IF_NAMESIZE] = "?";

			if_indextoname(r.oif[i].ifindex, oif);
			fprintf(f, " %s", oif);
		}
		fprintf(f, " packets %llu bytes %llu\n",
			(unsigned long long)r.packets,
			(unsigned long long)r.bytes);
	}
	fclose(f);
	rename(MCFAST_STATUS ".tmp", MCFAST_STATUS);
}

static void resync(int dump_fd)
{
	int n_mfc, n_want = 0;

	n_mfc = dump_mfc(dump_fd);
	if (n_mfc < 0) {
		fprintf(stderr, "mcfast: MFC dump: %s\n", strerror(-n_mfc));
		return;
	}
	for (int i = 0; i < n_mfc; i++)
		if (build_want(&mfcs[i], &wants[n_want]))
			n_want++;
	sync_links(n_want);
	sync_map(n_want);
	write_status();
}

static void drain(int fd)
{
	static char buf[16384];

	while (recv(fd, buf, sizeof(buf), MSG_DONTWAIT) > 0)
		;
}

static void usage(const char *name)
{
	fprintf(stderr,
		"usage: %s [-o bpf-object] [-i interval-ms] [-v]\n", name);
}

int main(int argc, char **argv)
{
	const char *path = MCFAST_OBJ;
	int interval = 1000, dump_fd, ev_fd, opt;
	struct bpf_map *map;

	while ((opt = getopt(argc, argv, "o:i:v")) != -1) {
		switch (opt) {
		case 'o':
			path = optarg;
			break;
		case 'i':
			interval = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage(argv[0]);
			return 2;
		}
	}

	obj = bpf_object__open_file(path, NULL);
	if (!obj || bpf_object__load(obj)) {
		fprintf(stderr, "mcfast: cannot load %s: %s\n", path,
			strerror(errno));
		return 1;
	}
	prog_eth = bpf_object__find_program_by_name(obj, "mcfast_eth");
	prog_ip = bpf_object__find_program_by_name(obj, "mcfast_ip");
	map = bpf_object__find_map_by_name(obj, "mcfast_routes");
	if (!prog_eth || !prog_ip || !map) {
		fprintf(stderr, "mcfast: %s lacks programs or map\n", path);
		return 1;
	}
	map_fd = bpf_map__fd(map);

	dump_fd = nl_open(0);
	ev_fd = nl_open(RTMGRP_IPV4_MROUTE | RTMGRP_LINK);
	if (dump_fd < 0 || ev_fd < 0) {
		perror("mcfast: netlink");
		return 1;
	}

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	while (!stop) {
		struct pollfd pfd = { .fd = ev_fd, .events = POLLIN };

		resync(dump_fd);
		if (poll(&pfd, 1, interval) > 0)
			drain(ev_fd);
	}

	for (int i = 0; i < n_links; i++)
		bpf_link__destroy(links[i].link);
	bpf_object__close(obj);
	unlink(MCFAST_STATUS);
	return 0;
}
