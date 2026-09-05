#define _GNU_SOURCE

#include <errno.h>
#include <inttypes.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NS_PER_SEC 1000000000LL
#define NS_PER_MS 1000000LL

static int64_t timespec_ns(const struct timespec *ts)
{
	return (int64_t)ts->tv_sec * NS_PER_SEC + ts->tv_nsec;
}

static int run_cpu_worker(unsigned int cpu, unsigned int seconds)
{
	cpu_set_t set;
	struct timespec start, now;
	uint64_t state = 0x9e3779b97f4a7c15ULL ^ cpu;
	uint64_t loops = 0;

	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	if (sched_setaffinity(0, sizeof(set), &set)) {
		perror("sched_setaffinity");
		return 1;
	}
	if (clock_gettime(CLOCK_MONOTONIC, &start)) {
		perror("clock_gettime");
		return 1;
	}
	do {
		for (unsigned int i = 0; i < 100000; i++) {
			state ^= state << 13;
			state ^= state >> 7;
			state ^= state << 17;
			loops++;
		}
		if (clock_gettime(CLOCK_MONOTONIC, &now)) {
			perror("clock_gettime");
			return 1;
		}
	} while (timespec_ns(&now) - timespec_ns(&start) <
		 (int64_t)seconds * NS_PER_SEC);

	printf("CPU cpu=%u observed_cpu=%d seconds=%u loops=%" PRIu64
	       " checksum=%016" PRIx64 " PASS\n",
	       cpu, sched_getcpu(), seconds, loops, state);
	fflush(stdout);
	return sched_getcpu() == (int)cpu ? 0 : 1;
}

static int test_cpus(unsigned int seconds)
{
	pid_t children[2];
	int failures = 0;

	for (unsigned int cpu = 0; cpu < 2; cpu++) {
		children[cpu] = fork();
		if (children[cpu] < 0) {
			perror("fork");
			return 1;
		}
		if (!children[cpu])
			_exit(run_cpu_worker(cpu, seconds));
	}
	for (unsigned int cpu = 0; cpu < 2; cpu++) {
		int status;

		if (waitpid(children[cpu], &status, 0) < 0 ||
		    !WIFEXITED(status) || WEXITSTATUS(status))
			failures++;
	}
	return failures != 0;
}

static int test_timer(void)
{
	const unsigned int samples = 1000;
	const int64_t interval = NS_PER_MS;
	struct timespec res, start, deadline, now;
	int64_t total_late = 0;
	int64_t max_late = 0;
	int64_t elapsed;

	if (clock_getres(CLOCK_MONOTONIC, &res) ||
	    clock_gettime(CLOCK_MONOTONIC, &start)) {
		perror("clock");
		return 1;
	}
	deadline = start;
	for (unsigned int i = 0; i < samples; i++) {
		int64_t deadline_ns = timespec_ns(&deadline) + interval;
		int ret;

		deadline.tv_sec = deadline_ns / NS_PER_SEC;
		deadline.tv_nsec = deadline_ns % NS_PER_SEC;
		do {
			ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
					      &deadline, NULL);
		} while (ret == EINTR);
		if (ret) {
			errno = ret;
			perror("clock_nanosleep");
			return 1;
		}
		if (clock_gettime(CLOCK_MONOTONIC, &now)) {
			perror("clock_gettime");
			return 1;
		}
		int64_t late = timespec_ns(&now) - deadline_ns;
		if (late < 0)
			return 1;
		total_late += late;
		if (late > max_late)
			max_late = late;
	}
	elapsed = timespec_ns(&now) - timespec_ns(&start);
	printf("TIMER resolution_ns=%" PRId64 " samples=%u elapsed_ms=%.3f "
	       "avg_late_us=%.3f max_late_us=%.3f %s\n",
	       timespec_ns(&res), samples, elapsed / 1e6,
	       total_late / (double)samples / 1e3, max_late / 1e3,
	       elapsed >= 990 * NS_PER_MS && elapsed <= 1250 * NS_PER_MS &&
	       max_late < 100 * NS_PER_MS ? "PASS" : "FAIL");
	return !(elapsed >= 990 * NS_PER_MS && elapsed <= 1250 * NS_PER_MS &&
		 max_late < 100 * NS_PER_MS);
}

static int verify_byte_pattern(const uint8_t *mem, size_t size, uint8_t value)
{
	for (size_t i = 0; i < size; i++)
		if (mem[i] != value) {
			printf("MEM byte mismatch offset=%zu expected=%02x got=%02x\n",
			       i, value, mem[i]);
			return 1;
		}
	return 0;
}

static uint64_t word_pattern(size_t index)
{
	return 0xaaaaaaaa55555555ULL ^
	       ((uint64_t)index * 0x9e3779b97f4a7c15ULL);
}

static int test_memory(unsigned int memory_mib)
{
	size_t size = (size_t)memory_mib * 1024 * 1024;
	size_t words = size / sizeof(uint64_t);
	uint64_t *mem;

	if (posix_memalign((void **)&mem, 4096, size)) {
		perror("posix_memalign");
		return 1;
	}
	memset(mem, 0x00, size);
	if (verify_byte_pattern((uint8_t *)mem, size, 0x00))
		goto fail;
	memset(mem, 0xff, size);
	if (verify_byte_pattern((uint8_t *)mem, size, 0xff))
		goto fail;
	for (size_t i = 0; i < words; i++)
		mem[i] = word_pattern(i);
	for (size_t i = 0; i < words; i++)
		if (mem[i] != word_pattern(i)) {
			printf("MEM word mismatch index=%zu expected=%016" PRIx64
			       " got=%016" PRIx64 "\n",
			       i, word_pattern(i), mem[i]);
			goto fail;
		}
	for (size_t i = 0; i < words; i++)
		mem[i] = ~word_pattern(i);
	for (size_t i = 0; i < words; i++)
		if (mem[i] != ~word_pattern(i)) {
			printf("MEM inverse mismatch index=%zu expected=%016" PRIx64
			       " got=%016" PRIx64 "\n",
			       i, ~word_pattern(i), mem[i]);
			goto fail;
		}
	free(mem);
	printf("MEM size_mib=%u patterns=4 PASS\n", memory_mib);
	return 0;
fail:
	free(mem);
	return 1;
}

int main(int argc, char **argv)
{
	unsigned int memory_mib = 320;
	unsigned int cpu_seconds = 5;
	int failed = 0;

	if (argc > 1)
		memory_mib = strtoul(argv[1], NULL, 0);
	if (argc > 2)
		cpu_seconds = strtoul(argv[2], NULL, 0);
	if (!memory_mib || !cpu_seconds) {
		fprintf(stderr, "usage: %s [memory-MiB] [cpu-seconds]\n", argv[0]);
		return 2;
	}
	setvbuf(stdout, NULL, _IOLBF, 0);
	printf("PLATFORM_CORE_TEST memory_mib=%u cpu_seconds=%u\n",
	       memory_mib, cpu_seconds);
	failed |= test_cpus(cpu_seconds);
	failed |= test_timer();
	failed |= test_memory(memory_mib);
	printf("PLATFORM_CORE_TEST %s\n", failed ? "FAIL" : "PASS");
	return failed;
}
