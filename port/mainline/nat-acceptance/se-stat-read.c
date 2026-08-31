// SPDX-License-Identifier: GPL-2.0-only

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define PPS_SMMU0_BASE		0x18048000UL
#define PPS_SMMU0_SIZE		0x1000
#define SMMU0_CMD		0x04
#define SMMU0_ADDR		0x08
#define SMMU0_DONE		0x1c
#define SMMU0_RDATA		0x24
#define SMMU0_READ64		0x08000000
#define SMMU0_CMD_MASK		0xfffff800
#define SMMU0_ADDR_MASK		0x0fffffff
#define FAST_STAT_BASE_BLOCK	0x9081
#define FAST_STAT_DEPTH		1024

static volatile uint32_t *regs;

static int smmu0_wait(void)
{
	unsigned int i;

	for (i = 0; i < 1000000; i++)
		if (regs[SMMU0_DONE / 4] & 3)
			return 0;
	return -ETIMEDOUT;
}

static int counter_read(uint32_t index, uint64_t *value)
{
	uint32_t addr, cmd;

	if (smmu0_wait())
		return -ETIMEDOUT;
	addr = (FAST_STAT_BASE_BLOCK << 7) + index * 64;
	regs[SMMU0_ADDR / 4] = (regs[SMMU0_ADDR / 4] & ~SMMU0_ADDR_MASK) |
				(addr & SMMU0_ADDR_MASK);
	cmd = regs[SMMU0_CMD / 4];
	regs[SMMU0_CMD / 4] = (cmd & ~SMMU0_CMD_MASK) | SMMU0_READ64;
	if (smmu0_wait())
		return -ETIMEDOUT;
	*value = (uint64_t)regs[SMMU0_RDATA / 4] << 32 |
		 regs[(SMMU0_RDATA + 4) / 4];

	return 0;
}

int main(int argc, char **argv)
{
	unsigned long flow_id;
	uint64_t packets, bytes;
	int scan_all;
	char *end;
	int fd;

	if (argc != 2) {
		fprintf(stderr, "usage: %s FLOW_ID|all\n", argv[0]);
		return 2;
	}
	scan_all = !strcmp(argv[1], "all");
	errno = 0;
	flow_id = scan_all ? 0 : strtoul(argv[1], &end, 0);
	if (!scan_all && (errno || *end || flow_id >= FAST_STAT_DEPTH)) {
		fprintf(stderr, "invalid flow id: %s\n", argv[1]);
		return 2;
	}
	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("open /dev/mem");
		return 1;
	}
	regs = mmap(NULL, PPS_SMMU0_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
		    fd, PPS_SMMU0_BASE);
	if (regs == MAP_FAILED) {
		perror("mmap SMMU0");
		return 1;
	}
	do {
		if (counter_read(2 * flow_id, &packets) ||
		    counter_read(2 * flow_id + 1, &bytes)) {
			fprintf(stderr, "SMMU0 read timed out\n");
			return 1;
		}
		if (!scan_all || packets || bytes)
			printf("flow=%lu packets=%" PRIu64 " bytes=%" PRIu64 "\n",
			       flow_id, packets, bytes);
		flow_id++;
	} while (scan_all && flow_id < FAST_STAT_DEPTH);

	return 0;
}
