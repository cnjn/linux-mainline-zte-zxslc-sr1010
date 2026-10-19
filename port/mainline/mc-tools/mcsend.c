/* mcsend GROUP PORT SRCADDR MBPS SECS [LEN] [TTL]: paced multicast UDP with a
 * 64-bit sequence number at the start of each payload. */
#include <arpa/inet.h>
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
	int len = argc > 6 ? atoi(argv[6]) : 1316, ttl = argc > 7 ? atoi(argv[7]) : 16;
	double mbps = atof(argv[4]), secs = atof(argv[5]), t0, next, gap;
	struct sockaddr_in dst = { .sin_family = AF_INET }, src = { .sin_family = AF_INET };
	struct in_addr ifa;
	char buf[9000] = {};
	uint64_t seq = 0, errs = 0;
	int s;

	if (argc < 6) {
		fprintf(stderr, "usage: mcsend GROUP PORT SRCADDR MBPS SECS [LEN] [TTL]\n");
		return 2;
	}
	s = socket(AF_INET, SOCK_DGRAM, 0);
	inet_pton(AF_INET, argv[1], &dst.sin_addr);
	dst.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, argv[3], &src.sin_addr);
	ifa = src.sin_addr;
	if (bind(s, (void *)&src, sizeof(src)) ||
	    setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &ifa, sizeof(ifa)) ||
	    setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))) {
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
