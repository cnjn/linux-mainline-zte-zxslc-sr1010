// SPDX-License-Identifier: GPL-2.0-only

#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static volatile sig_atomic_t stopped;

static void stop(int signo)
{
	(void)signo;
	stopped = 1;
}

int main(int argc, char **argv)
{
	struct sockaddr_in local = {
		.sin_family = AF_INET,
	};
	struct sockaddr_in peer;
	struct pollfd pfd;
	uint64_t packets = 0;
	uint64_t bytes = 0;
	socklen_t peer_len;
	unsigned char buf[65536];
	unsigned long port;
	ssize_t len;
	int fd;
	int one = 1;

	if (argc != 2) {
		fprintf(stderr, "usage: %s PORT\n", argv[0]);
		return 2;
	}
	port = strtoul(argv[1], NULL, 10);
	if (!port || port > 65535)
		return 2;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	local.sin_port = htons(port);
	if (inet_pton(AF_INET, "192.168.1.100", &local.sin_addr) != 1 ||
	    bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
		perror("bind");
		close(fd);
		return 1;
	}

	signal(SIGINT, stop);
	signal(SIGTERM, stop);
	pfd.fd = fd;
	pfd.events = POLLIN;
	while (!stopped) {
		if (poll(&pfd, 1, 200) <= 0)
			continue;
		peer_len = sizeof(peer);
		len = recvfrom(fd, buf, sizeof(buf), 0,
			       (struct sockaddr *)&peer, &peer_len);
		if (len < 0) {
			if (errno == EINTR)
				continue;
			perror("recvfrom");
			break;
		}
		if (sendto(fd, buf, len, 0, (struct sockaddr *)&peer,
			   peer_len) != len) {
			perror("sendto");
			break;
		}
		packets++;
		bytes += len;
	}

	printf("port=%lu packets=%llu bytes=%llu\n", port,
	       (unsigned long long)packets, (unsigned long long)bytes);
	close(fd);
	return 0;
}
