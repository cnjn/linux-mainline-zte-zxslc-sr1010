#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define THREADS 4
#define PAYLOAD_LEN 1472

struct worker {
	int fd;
	struct sockaddr_in remote;
	uint64_t start_ns;
	uint64_t end_ns;
	double target_bps;
	uint64_t packets;
	uint64_t bytes;
	uint64_t errors;
};

static uint64_t now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static void *send_worker(void *arg)
{
	struct worker *worker = arg;
	char payload[PAYLOAD_LEN] = { 0 };

	for (;;) {
		uint64_t current_ns = now_ns();
		double allowed;
		ssize_t sent;

		if (current_ns >= worker->end_ns)
			return NULL;
		allowed = (current_ns - worker->start_ns) *
			  worker->target_bps / 8000000000.0;
		if (worker->bytes + sizeof(payload) > allowed)
			continue;

		sent = sendto(worker->fd, payload, sizeof(payload), 0,
			      (struct sockaddr *)&worker->remote,
			      sizeof(worker->remote));
		if (sent > 0) {
			worker->packets++;
			worker->bytes += (uint64_t)sent;
		} else if (errno != ENOBUFS) {
			worker->errors++;
		}
	}
}

int main(int argc, char **argv)
{
	struct sockaddr_in local = { 0 };
	struct sockaddr_in remote = { 0 };
	struct worker workers[THREADS] = { 0 };
	pthread_t threads[THREADS];
	uint64_t packets = 0, bytes = 0, errors = 0;
	uint64_t start_ns, end_ns;
	int seconds = 30;
	int sendbuf = 4 * 1024 * 1024;
	int one = 1;
	double target_link_bps = 2460000000.0;
	double target_payload_bps;

	if (argc >= 2)
		seconds = atoi(argv[1]);
	if (argc >= 3)
		target_link_bps = atof(argv[2]);
	target_payload_bps = target_link_bps * PAYLOAD_LEN /
			     (PAYLOAD_LEN + 42.0);

	local.sin_family = AF_INET;
	local.sin_port = htons(5202);
	inet_pton(AF_INET, "192.168.1.100", &local.sin_addr);
	remote.sin_family = AF_INET;
	remote.sin_port = htons(5202);
	inet_pton(AF_INET, "192.168.1.1", &remote.sin_addr);

	for (unsigned int i = 0; i < THREADS; i++) {
		workers[i].fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (workers[i].fd < 0)
			return 1;
		setsockopt(workers[i].fd, SOL_SOCKET, SO_REUSEADDR,
			   &one, sizeof(one));
		setsockopt(workers[i].fd, SOL_SOCKET, SO_REUSEPORT,
			   &one, sizeof(one));
		setsockopt(workers[i].fd, SOL_SOCKET, SO_SNDBUF,
			   &sendbuf, sizeof(sendbuf));
		if (bind(workers[i].fd, (struct sockaddr *)&local,
			 sizeof(local)) < 0)
			return 2;
		workers[i].remote = remote;
	}

	start_ns = now_ns();
	end_ns = start_ns + (uint64_t)seconds * 1000000000ULL;
	for (unsigned int i = 0; i < THREADS; i++) {
		workers[i].start_ns = start_ns;
		workers[i].end_ns = end_ns;
		workers[i].target_bps = target_payload_bps / THREADS;
		pthread_create(&threads[i], NULL, send_worker, &workers[i]);
	}

	for (unsigned int i = 0; i < THREADS; i++) {
		pthread_join(threads[i], NULL);
		packets += workers[i].packets;
		bytes += workers[i].bytes;
		errors += workers[i].errors;
		close(workers[i].fd);
	}
	end_ns = now_ns();

	double elapsed = (end_ns - start_ns) / 1000000000.0;
	printf("packets=%llu bytes=%llu errors=%llu seconds=%.6f "
	       "payload_bps=%.0f\n",
	       packets, bytes, errors, elapsed, bytes * 8.0 / elapsed);
	return 0;
}
