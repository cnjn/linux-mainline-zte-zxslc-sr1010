/* mcsend6 GROUP PORT SRCADDR IFNAME MBPS SECS [LEN] [HOPS]: paced IPv6 multicast
 * UDP with a 64-bit sequence number at the start of each payload. */
#include <arpa/inet.h>
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static double now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
	int len = argc > 7 ? atoi(argv[7]) : 1316, hops = argc > 8 ? atoi(argv[8]) : 16;
	double mbps, secs, t0, next, gap;
	struct sockaddr_in6 dst = { .sin6_family = AF_INET6 }, src = { .sin6_family = AF_INET6 };
	char buf[9000] = {};
	uint64_t seq = 0, errs = 0;
	unsigned int idx;
	int s;

	if (argc < 7) {
		fprintf(stderr, "usage: mcsend6 GROUP PORT SRCADDR IFNAME MBPS SECS [LEN] [HOPS]\n");
		return 2;
	}
	mbps = atof(argv[5]);
	secs = atof(argv[6]);
	s = socket(AF_INET6, SOCK_DGRAM, 0);
	inet_pton(AF_INET6, argv[1], &dst.sin6_addr);
	dst.sin6_port = htons(atoi(argv[2]));
	inet_pton(AF_INET6, argv[3], &src.sin6_addr);
	idx = if_nametoindex(argv[4]);
	if (bind(s, (void *)&src, sizeof(src)) ||
	    setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_IF, &idx, sizeof(idx)) ||
	    setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops))) {
		perror("socket setup");
		return 1;
	}
	gap = len * 8.0 / (mbps * 1e6);
	t0 = next = now();
	while (now() - t0 < secs) {
		memcpy(buf, &seq, sizeof(seq));
		if (sendto(s, buf, len, 0, (void *)&dst, sizeof(dst)) < 0)
			errs++;
		seq++;
		next += gap;
		while (now() < next)
			;
	}
	printf("sent %llu packets (%llu errors) in %.2f s, %.1f Mbit/s payload\n",
	       (unsigned long long)seq, (unsigned long long)errs, now() - t0,
	       seq * len * 8 / (now() - t0) / 1e6);
	return 0;
}
