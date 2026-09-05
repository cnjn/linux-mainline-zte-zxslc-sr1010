// SPDX-License-Identifier: GPL-2.0-only
#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_bridge.h>
#include <linux/if_ether.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef MDB_FLAGS_OFFLOAD_FAILED
#define MDB_FLAGS_OFFLOAD_FAILED	(1 << 4)
#endif

static int addattr(struct nlmsghdr *nlh, size_t maxlen, unsigned int type,
		   const void *data, size_t len)
{
	size_t offset = NLMSG_ALIGN(nlh->nlmsg_len);
	size_t attr_len = RTA_LENGTH(len);
	struct rtattr *rta;

	if (offset + RTA_ALIGN(attr_len) > maxlen)
		return -EMSGSIZE;
	rta = (struct rtattr *)((char *)nlh + offset);
	rta->rta_type = type;
	rta->rta_len = attr_len;
	memcpy(RTA_DATA(rta), data, len);
	nlh->nlmsg_len = offset + RTA_ALIGN(attr_len);
	return 0;
}

static int open_netlink(void)
{
	struct sockaddr_nl local = { .nl_family = AF_NETLINK };
	int fd;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (fd < 0)
		return -1;
	if (bind(fd, (struct sockaddr *)&local, sizeof(local))) {
		close(fd);
		return -1;
	}
	return fd;
}

static int netlink_ack(int fd, unsigned int seq)
{
	char buf[4096];
	ssize_t len;

	for (;;) {
		struct nlmsghdr *nlh;

		len = recv(fd, buf, sizeof(buf), 0);
		if (len < 0)
			return -errno;
		for (nlh = (struct nlmsghdr *)buf; NLMSG_OK(nlh, len);
		     nlh = NLMSG_NEXT(nlh, len)) {
			struct nlmsgerr *err;

			if (nlh->nlmsg_seq != seq)
				continue;
			if (nlh->nlmsg_type != NLMSG_ERROR)
				continue;
			if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(*err)))
				return -EPROTO;
			err = NLMSG_DATA(nlh);
			return err->error;
		}
	}
}

static int parse_group(const char *group, struct br_mdb_entry *entry)
{
	unsigned int mac[ETH_ALEN];

	if (inet_pton(AF_INET, group, &entry->addr.u.ip4) == 1) {
		entry->addr.proto = htons(ETH_P_IP);
		return 0;
	}
	if (inet_pton(AF_INET6, group, &entry->addr.u.ip6) == 1) {
		entry->addr.proto = htons(ETH_P_IPV6);
		return 0;
	}
	if (sscanf(group, "%x:%x:%x:%x:%x:%x", &mac[0], &mac[1], &mac[2],
		   &mac[3], &mac[4], &mac[5]) != ETH_ALEN)
		return -EINVAL;
	for (unsigned int i = 0; i < ETH_ALEN; i++) {
		if (mac[i] > 0xff)
			return -EINVAL;
		entry->addr.u.mac_addr[i] = mac[i];
	}
	return 0;
}

static int update_mdb(bool add, const char *bridge, const char *port,
		      const char *group, unsigned int vid)
{
	static unsigned int seq;
	struct {
		struct nlmsghdr nlh;
		struct br_port_msg bpm;
		char attrs[256];
	} req = {};
	struct sockaddr_nl kernel = { .nl_family = AF_NETLINK };
	struct br_mdb_entry entry = { .state = MDB_PERMANENT };
	unsigned int bridge_index;
	int fd;
	int ret;

	bridge_index = if_nametoindex(bridge);
	entry.ifindex = if_nametoindex(port);
	if (!bridge_index || !entry.ifindex)
		return -ENODEV;
	if (vid > 4095)
		return -EINVAL;
	entry.vid = vid;
	ret = parse_group(group, &entry);
	if (ret)
		return ret;
	if (entry.ifindex == bridge_index && entry.addr.proto)
		entry.state = MDB_TEMPORARY;

	fd = open_netlink();
	if (fd < 0)
		return -errno;
	req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(req.bpm));
	req.nlh.nlmsg_type = add ? RTM_NEWMDB : RTM_DELMDB;
	req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	if (add)
		req.nlh.nlmsg_flags |= NLM_F_CREATE | NLM_F_EXCL;
	req.nlh.nlmsg_seq = ++seq;
	req.bpm.family = AF_BRIDGE;
	req.bpm.ifindex = bridge_index;
	ret = addattr(&req.nlh, sizeof(req), MDBA_SET_ENTRY, &entry,
		      sizeof(entry));
	if (!ret && sendto(fd, &req, req.nlh.nlmsg_len, 0,
			   (struct sockaddr *)&kernel, sizeof(kernel)) < 0)
		ret = -errno;
	if (!ret)
		ret = netlink_ack(fd, req.nlh.nlmsg_seq);
	close(fd);
	return ret;
}

static void print_entry(const struct br_mdb_entry *entry)
{
	char ifname[IF_NAMESIZE] = "?";
	char group[INET6_ADDRSTRLEN];

	if_indextoname(entry->ifindex, ifname);
	if (entry->addr.proto == htons(ETH_P_IP)) {
		inet_ntop(AF_INET, &entry->addr.u.ip4, group, sizeof(group));
	} else if (entry->addr.proto == htons(ETH_P_IPV6)) {
		inet_ntop(AF_INET6, &entry->addr.u.ip6, group, sizeof(group));
	} else {
		snprintf(group, sizeof(group), "%02x:%02x:%02x:%02x:%02x:%02x",
			 entry->addr.u.mac_addr[0], entry->addr.u.mac_addr[1],
			 entry->addr.u.mac_addr[2], entry->addr.u.mac_addr[3],
			 entry->addr.u.mac_addr[4], entry->addr.u.mac_addr[5]);
	}
	printf("port=%s group=%s vid=%u state=%s offload=%u failed=%u\n",
	       ifname, group, entry->vid,
	       entry->state == MDB_PERMANENT ? "permanent" : "temporary",
	       !!(entry->flags & MDB_FLAGS_OFFLOAD),
	       !!(entry->flags & MDB_FLAGS_OFFLOAD_FAILED));
}

static void dump_entries(struct rtattr *attr)
{
	int len = RTA_PAYLOAD(attr);
	struct rtattr *entry_attr;

	for (entry_attr = RTA_DATA(attr); RTA_OK(entry_attr, len);
	     entry_attr = RTA_NEXT(entry_attr, len)) {
		struct rtattr *info_attr;
		int entry_len;

		if ((entry_attr->rta_type & NLA_TYPE_MASK) != MDBA_MDB_ENTRY)
			continue;
		entry_len = RTA_PAYLOAD(entry_attr);
		for (info_attr = RTA_DATA(entry_attr);
		     RTA_OK(info_attr, entry_len);
		     info_attr = RTA_NEXT(info_attr, entry_len)) {
			if ((info_attr->rta_type & NLA_TYPE_MASK) !=
			    MDBA_MDB_ENTRY_INFO ||
			    RTA_PAYLOAD(info_attr) < sizeof(struct br_mdb_entry))
				continue;
			print_entry(RTA_DATA(info_attr));
		}
	}
}

static int dump_mdb(const char *bridge)
{
	static unsigned int seq;
	struct {
		struct nlmsghdr nlh;
		struct br_port_msg bpm;
	} req = {};
	struct sockaddr_nl kernel = { .nl_family = AF_NETLINK };
	char buf[8192];
	unsigned int bridge_index;
	int fd;

	bridge_index = if_nametoindex(bridge);
	if (!bridge_index)
		return -ENODEV;
	fd = open_netlink();
	if (fd < 0)
		return -errno;
	req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(req.bpm));
	req.nlh.nlmsg_type = RTM_GETMDB;
	req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req.nlh.nlmsg_seq = ++seq;
	req.bpm.family = AF_BRIDGE;
	req.bpm.ifindex = bridge_index;
	if (sendto(fd, &req, req.nlh.nlmsg_len, 0,
		   (struct sockaddr *)&kernel, sizeof(kernel)) < 0) {
		close(fd);
		return -errno;
	}

	for (;;) {
		ssize_t len = recv(fd, buf, sizeof(buf), 0);
		struct nlmsghdr *nlh;

		if (len < 0) {
			close(fd);
			return -errno;
		}
		for (nlh = (struct nlmsghdr *)buf; NLMSG_OK(nlh, len);
		     nlh = NLMSG_NEXT(nlh, len)) {
			struct br_port_msg *bpm;
			struct rtattr *attr;
			int attr_len;

			if (nlh->nlmsg_seq != req.nlh.nlmsg_seq)
				continue;
			if (nlh->nlmsg_type == NLMSG_DONE) {
				close(fd);
				return 0;
			}
			if (nlh->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *err = NLMSG_DATA(nlh);

				close(fd);
				return err->error;
			}
			if ((nlh->nlmsg_type != RTM_GETMDB &&
			     nlh->nlmsg_type != RTM_NEWMDB) ||
			    nlh->nlmsg_len < NLMSG_LENGTH(sizeof(*bpm)))
				continue;
			bpm = NLMSG_DATA(nlh);
			attr_len = nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*bpm));
			for (attr = (struct rtattr *)((char *)bpm +
						       NLMSG_ALIGN(sizeof(*bpm)));
			     RTA_OK(attr, attr_len);
			     attr = RTA_NEXT(attr, attr_len))
				if ((attr->rta_type & NLA_TYPE_MASK) == MDBA_MDB)
					dump_entries(attr);
		}
	}
}

int main(int argc, char **argv)
{
	unsigned int vid = 0;
	int ret;

	if (argc == 3 && !strcmp(argv[1], "dump")) {
		ret = dump_mdb(argv[2]);
	} else if ((argc == 5 || argc == 6) &&
		   (!strcmp(argv[1], "add") || !strcmp(argv[1], "del"))) {
		if (argc == 6)
			vid = strtoul(argv[5], NULL, 0);
		ret = update_mdb(!strcmp(argv[1], "add"), argv[2], argv[3],
				 argv[4], vid);
	} else {
		fprintf(stderr,
			"usage: %s add|del BRIDGE PORT GROUP [VID]\n"
			"       %s dump BRIDGE\n", argv[0], argv[0]);
		return 2;
	}
	if (ret) {
		errno = -ret;
		perror("mdb netlink");
		return 1;
	}
	return 0;
}
