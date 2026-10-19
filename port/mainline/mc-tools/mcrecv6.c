/* mcrecv6 GROUP PORT IFNAME SECS: join the IPv6 GROUP on one interface only
 * and count packets, sequence gaps and duplicates. */
#include <arpa/inet.h>
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	struct sockaddr_in6 a = { .sin6_family = AF_INET6 };
	struct ipv6_mreq mr = {};
	struct timeval tv = { .tv_sec = 0, .tv_usec = 200000 };
	uint64_t seq, first = UINT64_MAX, max = 0, got = 0, dups = 0, back = 0;
	unsigned long long bytes = 0;
	time_t end, last = 0;
	char buf[9000];
	int s, one = 1, idx;
	ssize_t n;

	if (argc < 5) {
		fprintf(stderr, "usage: mcrecv6 GROUP PORT IFNAME SECS\n");
		return 2;
	}
	s = socket(AF_INET6, SOCK_DGRAM, 0);
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
	n = 8 << 20;
	setsockopt(s, SOL_SOCKET, SO_RCVBUF, &n, sizeof(int));
	idx = if_nametoindex(argv[3]);
#ifdef IPV6_BOUND_IF
	setsockopt(s, IPPROTO_IPV6, IPV6_BOUND_IF, &idx, sizeof(idx));
#endif
	inet_pton(AF_INET6, argv[1], &a.sin6_addr);
	a.sin6_port = htons(atoi(argv[2]));
	if (bind(s, (void *)&a, sizeof(a))) {
		perror("bind");
		return 1;
	}
	mr.ipv6mr_multiaddr = a.sin6_addr;
	mr.ipv6mr_interface = idx;
	if (setsockopt(s, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mr, sizeof(mr))) {
		perror("join");
		return 1;
	}
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	end = time(NULL) + atoi(argv[4]);
	while (time(NULL) < end) {
		n = recv(s, buf, sizeof(buf), 0);
		if (n < 8)
			continue;
		memcpy(&seq, buf, sizeof(seq));
		got++;
		bytes += n;
		if (first == UINT64_MAX)
			first = seq;
		if (seq > max || got == 1)
			max = seq;
		else if (seq == max)
			dups++;
		else
			back++;
		if (time(NULL) != last) {
			last = time(NULL);
		}
	}
	if (!got) {
		printf("%s: nothing received\n", argv[3]);
		return 0;
	}
	printf("%s: got %llu, span %llu, lost %lld (%.3f%%), dup %llu, reordered %llu, %.1f MB\n",
	       argv[3], (unsigned long long)got,
	       (unsigned long long)(max - first + 1),
	       (long long)(max - first + 1) - (long long)got,
	       100.0 * ((double)(max - first + 1) - got) / (max - first + 1),
	       (unsigned long long)dups, (unsigned long long)back, bytes / 1e6);
	return 0;
}
