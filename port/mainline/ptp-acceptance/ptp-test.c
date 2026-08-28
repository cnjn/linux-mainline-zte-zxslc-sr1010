// SPDX-License-Identifier: GPL-2.0-only

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/timex.h>
#include <time.h>
#include <unistd.h>

#include <net/if.h>

#define CLOCKFD 3
#define FD_TO_CLOCKID(fd) ((~(clockid_t)(fd) << 3) | CLOCKFD)

static int open_phc(const char *path)
{
	int fd = open(path, O_RDWR);

	if (fd < 0) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	return fd;
}

static int print_timestamp(struct msghdr *msg)
{
	struct cmsghdr *cmsg;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
		struct timespec *ts;

		if (cmsg->cmsg_level != SOL_SOCKET ||
		    cmsg->cmsg_type != SCM_TIMESTAMPING)
			continue;
		ts = (struct timespec *)CMSG_DATA(cmsg);
		printf("software=%lld.%09ld transformed=%lld.%09ld raw=%lld.%09ld\n",
		       (long long)ts[0].tv_sec, ts[0].tv_nsec,
		       (long long)ts[1].tv_sec, ts[1].tv_nsec,
		       (long long)ts[2].tv_sec, ts[2].tv_nsec);
		return ts[2].tv_sec || ts[2].tv_nsec ? 0 : 2;
	}
	fputs("no SCM_TIMESTAMPING control message\n", stderr);
	return 2;
}

static int set_hwtstamp(int sock, const char *ifname, int tx_type,
			int rx_filter)
{
	struct hwtstamp_config config = {
		.tx_type = tx_type,
		.rx_filter = rx_filter,
	};
	struct ifreq ifr = {};

	if (strlen(ifname) >= sizeof(ifr.ifr_name))
		return -EINVAL;
	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_data = (void *)&config;
	if (ioctl(sock, SIOCSHWTSTAMP, &ifr))
		return -errno;
	printf("hwtstamp tx_type=%d rx_filter=%d\n", config.tx_type,
	       config.rx_filter);
	return 0;
}

static int socket_timestamping(int sock, int flags)
{
	if (setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &flags,
		       sizeof(flags)))
		return -errno;
	return 0;
}

static int command_clock(const char *path, unsigned int count)
{
	struct timespec phc, mono;
	clockid_t clkid;
	int fd;

	fd = open_phc(path);
	clkid = FD_TO_CLOCKID(fd);
	while (count--) {
		if (clock_gettime(CLOCK_MONOTONIC_RAW, &mono) ||
		    clock_gettime(clkid, &phc)) {
			perror("clock_gettime");
			close(fd);
			return 1;
		}
		printf("phc=%lld.%09ld mono=%lld.%09ld\n",
		       (long long)phc.tv_sec, phc.tv_nsec,
		       (long long)mono.tv_sec, mono.tv_nsec);
		sleep(1);
	}
	close(fd);
	return 0;
}

static int command_set(const char *path, const char *sec_arg,
		       const char *nsec_arg)
{
	struct timespec ts = {
		.tv_sec = strtoll(sec_arg, NULL, 0),
		.tv_nsec = strtol(nsec_arg, NULL, 0),
	};
	clockid_t clkid;
	int fd;

	fd = open_phc(path);
	clkid = FD_TO_CLOCKID(fd);
	if (clock_settime(clkid, &ts)) {
		perror("clock_settime");
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}

static int command_adjfreq(const char *path, const char *ppb_arg)
{
	long long ppb = strtoll(ppb_arg, NULL, 0);
	struct timex tx = {
		.modes = ADJ_FREQUENCY,
		.freq = ppb * 65536 / 1000,
	};
	clockid_t clkid;
	int fd;

	fd = open_phc(path);
	clkid = FD_TO_CLOCKID(fd);
	if (clock_adjtime(clkid, &tx) < 0) {
		perror("clock_adjtime");
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}

static int command_tx(const char *ifname, const char *peer)
{
	uint8_t ptp[44] = { [0] = 0, [1] = 2, [3] = sizeof(ptp), [31] = 1 };
	struct sockaddr_in address = {
		.sin_family = AF_INET,
		.sin_port = htons(319),
	};
	struct pollfd pfd = { .events = POLLERR };
	char control[512], data[256];
	struct iovec iov = { .iov_base = data, .iov_len = sizeof(data) };
	struct msghdr msg = {
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = control,
		.msg_controllen = sizeof(control),
	};
	int flags = SOF_TIMESTAMPING_TX_HARDWARE |
		SOF_TIMESTAMPING_RAW_HARDWARE;
	int ret, sock;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		return 1;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, ifname,
		       strlen(ifname) + 1)) {
		perror("SO_BINDTODEVICE");
		goto fail;
	}
	ret = set_hwtstamp(sock, ifname, HWTSTAMP_TX_ON,
			   HWTSTAMP_FILTER_NONE);
	if (ret) {
		errno = -ret;
		perror("SIOCSHWTSTAMP");
		goto fail;
	}
	ret = socket_timestamping(sock, flags);
	if (ret) {
		errno = -ret;
		perror("SO_TIMESTAMPING");
		goto fail;
	}
	if (inet_pton(AF_INET, peer, &address.sin_addr) != 1) {
		fputs("invalid peer address\n", stderr);
		goto fail;
	}
	if (sendto(sock, ptp, sizeof(ptp), 0, (struct sockaddr *)&address,
		   sizeof(address)) < 0) {
		perror("sendto");
		goto fail;
	}
	pfd.fd = sock;
	ret = poll(&pfd, 1, 2000);
	if (ret <= 0) {
		if (!ret)
			fputs("TX timestamp timeout\n", stderr);
		else
			perror("poll");
		goto fail;
	}
	if (recvmsg(sock, &msg, MSG_ERRQUEUE) < 0) {
		perror("recvmsg(MSG_ERRQUEUE)");
		goto fail;
	}
	ret = print_timestamp(&msg);
	close(sock);
	return ret;

fail:
	close(sock);
	return 1;
}

static int command_rx(const char *ifname)
{
	struct sockaddr_in address = {
		.sin_family = AF_INET,
		.sin_port = htons(319),
		.sin_addr.s_addr = htonl(INADDR_ANY),
	};
	struct pollfd pfd = { .events = POLLIN };
	char control[512], data[2048];
	struct iovec iov = { .iov_base = data, .iov_len = sizeof(data) };
	struct msghdr msg = {
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = control,
		.msg_controllen = sizeof(control),
	};
	int flags = SOF_TIMESTAMPING_RX_HARDWARE |
		SOF_TIMESTAMPING_RAW_HARDWARE;
	int ret, sock;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		return 1;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, ifname,
		       strlen(ifname) + 1)) {
		perror("SO_BINDTODEVICE");
		goto fail;
	}
	ret = set_hwtstamp(sock, ifname, HWTSTAMP_TX_OFF,
			   HWTSTAMP_FILTER_PTP_V2_L4_EVENT);
	if (ret) {
		errno = -ret;
		perror("SIOCSHWTSTAMP");
		goto fail;
	}
	ret = socket_timestamping(sock, flags);
	if (ret) {
		errno = -ret;
		perror("SO_TIMESTAMPING");
		goto fail;
	}
	if (bind(sock, (struct sockaddr *)&address, sizeof(address))) {
		perror("bind");
		goto fail;
	}
	pfd.fd = sock;
	ret = poll(&pfd, 1, 10000);
	if (ret <= 0) {
		if (!ret)
			fputs("RX timestamp timeout\n", stderr);
		else
			perror("poll");
		goto fail;
	}
	if (recvmsg(sock, &msg, 0) < 0) {
		perror("recvmsg");
		goto fail;
	}
	ret = print_timestamp(&msg);
	close(sock);
	return ret;

fail:
	close(sock);
	return 1;
}

static void usage(const char *name)
{
	fprintf(stderr,
		"usage:\n"
		"  %s clock <ptp-device> [samples]\n"
		"  %s set <ptp-device> <sec> <nsec>\n"
		"  %s adjfreq <ptp-device> <ppb>\n"
		"  %s tx <interface> <peer-ipv4>\n"
		"  %s rx <interface>\n",
		name, name, name, name, name);
}

int main(int argc, char **argv)
{
	if (argc >= 3 && !strcmp(argv[1], "clock"))
		return command_clock(argv[2], argc > 3 ? strtoul(argv[3], NULL, 0) : 5);
	if (argc == 5 && !strcmp(argv[1], "set"))
		return command_set(argv[2], argv[3], argv[4]);
	if (argc == 4 && !strcmp(argv[1], "adjfreq"))
		return command_adjfreq(argv[2], argv[3]);
	if (argc == 4 && !strcmp(argv[1], "tx"))
		return command_tx(argv[2], argv[3]);
	if (argc == 3 && !strcmp(argv[1], "rx"))
		return command_rx(argv[2]);

	usage(argv[0]);
	return 2;
}
