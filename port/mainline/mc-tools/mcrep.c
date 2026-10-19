/* mcrep GROUP IFNAME SECS: send a membership report for GROUP every 5 s for
 * SECS seconds as a raw Ethernet frame on IFNAME (root, macOS /dev/bpf):
 * IGMPv2 for an IPv4 group, MLDv1 for an IPv6 one, both with Router Alert
 * as RFC 2236 / 2710 ask (omcproxy drops reports without it; the macOS IP
 * stack drops locally built packets that carry IP options). */
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static uint32_t sum16(const void *p, int n, uint32_t s)
{
	const uint8_t *b = p;

	for (; n > 1; n -= 2, b += 2)
		s += b[0] << 8 | b[1];
	if (n)
		s += b[0] << 8;
	return s;
}

static uint16_t fold(uint32_t s)
{
	while (s >> 16)
		s = (s & 0xffff) + (s >> 16);
	return htons(~s & 0xffff);
}

int main(int argc, char **argv)
{
	uint8_t f[128] = {}, mac[6] = {};
	struct in6_addr g6, ll6 = {};
	struct in_addr g4, a4 = {};
	struct ifaddrs *ifs, *i;
	struct ifreq ifr = {};
	int v6, len, fd = -1, n, secs;
	char dev[16];

	if (argc < 4) {
		fprintf(stderr, "usage: mcrep GROUP IFNAME SECS\n");
		return 2;
	}
	secs = atoi(argv[3]);
	v6 = strchr(argv[1], ':') != NULL;
	if (v6 ? inet_pton(AF_INET6, argv[1], &g6) != 1 : inet_pton(AF_INET, argv[1], &g4) != 1) {
		fprintf(stderr, "bad group %s\n", argv[1]);
		return 2;
	}
	getifaddrs(&ifs);
	for (i = ifs; i; i = i->ifa_next) {
		if (!i->ifa_addr || strcmp(i->ifa_name, argv[2]))
			continue;
		if (i->ifa_addr->sa_family == AF_LINK)
			memcpy(mac, LLADDR((struct sockaddr_dl *)i->ifa_addr), 6);
		else if (i->ifa_addr->sa_family == AF_INET && !a4.s_addr)
			a4 = ((struct sockaddr_in *)i->ifa_addr)->sin_addr;
		else if (i->ifa_addr->sa_family == AF_INET6 &&
			 IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6 *)i->ifa_addr)->sin6_addr)) {
			ll6 = ((struct sockaddr_in6 *)i->ifa_addr)->sin6_addr;
			ll6.s6_addr[2] = ll6.s6_addr[3] = 0;	/* KAME scope id */
		}
	}

	memcpy(f + 6, mac, 6);
	if (!v6) {
		uint32_t g = ntohl(g4.s_addr);
		uint8_t dmac[6] = { 1, 0, 0x5e, (g >> 16) & 0x7f, g >> 8, g };

		memcpy(f, dmac, 6);
		f[12] = 0x08;
		uint8_t *ip = f + 14, *igmp = ip + 24;
		ip[0] = 0x46; ip[3] = 32; ip[8] = 1; ip[9] = 2;
		memcpy(ip + 12, &a4, 4);
		memcpy(ip + 16, &g4, 4);
		ip[20] = 0x94; ip[21] = 4;
		*(uint16_t *)(ip + 10) = fold(sum16(ip, 24, 0));
		igmp[0] = 0x16;
		memcpy(igmp + 4, &g4, 4);
		*(uint16_t *)(igmp + 2) = fold(sum16(igmp, 8, 0));
		len = 14 + 32;
	} else {
		uint8_t dmac[6] = { 0x33, 0x33 };

		memcpy(dmac + 2, &g6.s6_addr[12], 4);
		memcpy(f, dmac, 6);
		f[12] = 0x86; f[13] = 0xdd;
		uint8_t *ip = f + 14, *hbh = ip + 40, *mld = hbh + 8;
		ip[0] = 0x60; ip[5] = 8 + 24; ip[6] = 0; ip[7] = 1;
		memcpy(ip + 8, &ll6, 16);
		memcpy(ip + 24, &g6, 16);
		hbh[0] = 58; hbh[2] = 5; hbh[3] = 2; hbh[6] = 1;
		mld[0] = 131;
		memcpy(mld + 8, &g6, 16);
		uint32_t s = sum16(ip + 8, 32, 0) + 24 + 58;
		*(uint16_t *)(mld + 2) = fold(sum16(mld, 24, s));
		len = 14 + 40 + 8 + 24;
	}

	for (n = 0; n < 64 && fd < 0; n++) {
		snprintf(dev, sizeof(dev), "/dev/bpf%d", n);
		fd = open(dev, O_WRONLY);
	}
	strlcpy(ifr.ifr_name, argv[2], sizeof(ifr.ifr_name));
	if (fd < 0 || ioctl(fd, BIOCSETIF, &ifr)) {
		perror("bpf");
		return 1;
	}
	for (n = 0; n <= secs; n += 5) {
		if (write(fd, f, len) != len)
			perror("write");
		sleep(5);
	}
	return 0;
}
