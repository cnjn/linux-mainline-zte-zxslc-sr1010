// SPDX-License-Identifier: GPL-2.0-only
/*
 * Userspace PPU single-step tracer for ZX279133, usable on both kernels:
 *   mainline: /sys/kernel/debug/zx279133-pps (raw 32-bit PPS window, pread/pwrite)
 *   vendor:   /dev/mem mmap of 0x18000000
 * Register layout from port/mainline/rss-acceptance/rx-ppu-trace.c.
 *
 * usage: ppu-trace pc=N [steps=N] [timeout=ms] [match=OFF:HEX[,OFF:HEX...]]
 *        ppu-trace dump=PC count=N      (read instruction SRAM)
 *        ppu-trace status               (stopped mask and the eight ME PCs, read only)
 *        ppu-trace watch=LO hi=HI secs=N inst=HEX  (report MEs stopped in [LO,HI) and
 *                                        resume them with instruction HEX)
 *        ppu-trace fill=PC count=N inst=HEX  (write one instruction over an SRAM range,
 *                                        PC and N multiples of 4)
 * OFF is a byte offset in the frame (packet word 32 = frame bytes 0..3).
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define PPS_PHYS	0x18000000UL
#define PPU		0x90000
#define GATE		0x1c
#define BKPT		0x0080000017d80000ULL
#define NWORDS		129

static int fd;
static volatile uint32_t *map;

static uint32_t rd(uint32_t off)
{
	uint32_t v;

	if (map)
		return map[off / 4];
	if (pread(fd, &v, 4, off) != 4) {
		perror("pread");
		exit(2);
	}
	return v;
}

static void wr(uint32_t off, uint32_t v)
{
	if (map) {
		map[off / 4] = v;
		return;
	}
	if (pwrite(fd, &v, 4, off) != 4) {
		perror("pwrite");
		exit(2);
	}
}

static void udelay_(long us)
{
	struct timespec ts = { 0, us * 1000 };

	nanosleep(&ts, NULL);
}

static long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int wait_bit0(uint32_t off)
{
	int i;

	for (i = 0; i < 10000; i++)
		if (rd(off) & 1)
			return 0;
	return -ETIMEDOUT;
}

static int command(uint32_t cmd)
{
	if (wait_bit0(PPU + 0xd04))
		return -ETIMEDOUT;
	wr(PPU + 0xd00, (rd(PPU + 0xd00) & ~0x1fffU) | cmd);
	return wait_bit0(PPU + 0xd00);
}

static int iblock(uint32_t addr, uint64_t *b, int write)
{
	int i;

	if (write) {
		if (wait_bit0(PPU + 0xd04))
			return -ETIMEDOUT;
		for (i = 0; i < 4; i++) {
			wr(PPU + 0xd0c + i * 8, (uint32_t)b[i]);
			wr(PPU + 0xd10 + i * 8,
			   (rd(PPU + 0xd10 + i * 8) & ~0x7ffffffU) |
			   (uint32_t)(b[i] >> 32));
		}
		if (command(addr >> 2))
			return -ETIMEDOUT;
	}
	if (command((1U << 12) | (addr >> 2)))
		return -ETIMEDOUT;
	for (i = 0; i < 4; i++) {
		uint64_t v = (uint64_t)(rd(PPU + 0xd5c + i * 8) & 0x7ffffff) << 32 |
			     rd(PPU + 0xd58 + i * 8);

		if (write && v != b[i])
			return -EIO;
		b[i] = v;
	}
	return 0;
}

static void latch(uint64_t inst)
{
	wr(PPU + 0xcc, (uint32_t)inst);
	wr(PPU + 0xd0, (uint32_t)(inst >> 32));
}

static uint32_t me_pc(int me)
{
	return rd(PPU + 0x100 + me * 4) & 0xffff;
}

static uint32_t stopped(void)
{
	return rd(PPU + 4) & 0xff;
}

static void words(int me, uint32_t *w)
{
	int i;

	for (i = 0; i < NWORDS; i++)
		w[i] = rd(PPU + 0x4400 + me * 0x400 + i * 4);
}

struct match {
	unsigned int off, len;
	uint8_t b[64];
};

static struct match m[16];
static int nm;

static int frame_byte(const uint32_t *w, unsigned int off)
{
	return w[32 + off / 4] >> (24 - 8 * (off % 4)) & 0xff;
}

static int matches(const uint32_t *w)
{
	int i;
	unsigned int j;

	for (i = 0; i < nm; i++)
		for (j = 0; j < m[i].len; j++)
			if (m[i].off + j >= 128 ||
			    frame_byte(w, m[i].off + j) != m[i].b[j])
				return 0;
	return 1;
}

static void parse_match(char *s)
{
	char *tok;

	for (tok = strtok(s, ","); tok && nm < 16; tok = strtok(NULL, ",")) {
		char *c = strchr(tok, ':');
		unsigned int k;

		if (!c)
			exit(1);
		m[nm].off = strtoul(tok, NULL, 0);
		c++;
		for (k = 0; c[2 * k] && c[2 * k + 1] && k < 64; k++) {
			char h[3] = { c[2 * k], c[2 * k + 1], 0 };

			m[nm].b[k] = strtoul(h, NULL, 16);
		}
		m[nm].len = k;
		nm++;
	}
}

static void print_words(const char *tag, const uint32_t *w, const uint32_t *prev)
{
	int i;

	printf("%s", tag);
	for (i = 0; i < NWORDS; i++)
		if (!prev || w[i] != prev[i])
			printf(" w%d=%08x", i, w[i]);
	printf("\n");
}

int main(int argc, char **argv)
{
	unsigned int pc = 0, steps = 400, timeout = 3000, dump = 0, dcount = 0;
	int have_pc = 0, have_dump = 0, i, me, sel = -1, ret = 0, seen = 0;
	uint64_t orig[4], blk[4];
	uint32_t gate, w[NWORDS], prev[NWORDS], mask;
	long t0;

	int status = 0, have_fill = 0;
	unsigned int fill = 0, watch = 0, whi = 0, secs = 10;
	int have_watch = 0;
	unsigned long long fill_inst = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "status")) {
			status = 1;
		} else if (!strncmp(argv[i], "pc=", 3)) {
			pc = strtoul(argv[i] + 3, NULL, 0);
			have_pc = 1;
		} else if (!strncmp(argv[i], "steps=", 6)) {
			steps = strtoul(argv[i] + 6, NULL, 0);
		} else if (!strncmp(argv[i], "timeout=", 8)) {
			timeout = strtoul(argv[i] + 8, NULL, 0);
		} else if (!strncmp(argv[i], "match=", 6)) {
			parse_match(argv[i] + 6);
		} else if (!strncmp(argv[i], "dump=", 5)) {
			dump = strtoul(argv[i] + 5, NULL, 0);
			have_dump = 1;
		} else if (!strncmp(argv[i], "watch=", 6)) {
			watch = strtoul(argv[i] + 6, NULL, 0);
			have_watch = 1;
		} else if (!strncmp(argv[i], "hi=", 3)) {
			whi = strtoul(argv[i] + 3, NULL, 0);
		} else if (!strncmp(argv[i], "secs=", 5)) {
			secs = strtoul(argv[i] + 5, NULL, 0);
		} else if (!strncmp(argv[i], "fill=", 5)) {
			fill = strtoul(argv[i] + 5, NULL, 0);
			have_fill = 1;
		} else if (!strncmp(argv[i], "inst=", 5)) {
			fill_inst = strtoull(argv[i] + 5, NULL, 16);
		} else if (!strncmp(argv[i], "count=", 6)) {
			dcount = strtoul(argv[i] + 6, NULL, 0);
		} else {
			fprintf(stderr, "bad arg %s\n", argv[i]);
			return 1;
		}
	}
	if ((!have_pc && !have_dump && !status && !have_fill && !have_watch) || pc >= 12288 || steps > 4096)
		return 1;

	fd = open("/sys/kernel/debug/zx279133-pps", O_RDWR | O_SYNC);
	if (fd < 0) {
		void *p;

		fd = open("/dev/mem", O_RDWR | O_SYNC);
		if (fd < 0) {
			perror("open");
			return 1;
		}
		p = mmap(NULL, 0x100000, PROT_READ | PROT_WRITE, MAP_SHARED,
			 fd, PPS_PHYS);
		if (p == MAP_FAILED) {
			perror("mmap");
			return 1;
		}
		map = p;
	}

	if (status) {
		printf("stopped=%02x pc", stopped());
		for (me = 0; me < 8; me++)
			printf(" %u", me_pc(me));
		printf("\n");
		return 0;
	}
	if (have_watch) {
		unsigned int hits = 0;
		long end = now_ms() + secs * 1000L;

		while (now_ms() < end) {
			mask = stopped();
			for (me = 0; me < 8; me++) {
				uint32_t a;

				if (!(mask & (1U << me)))
					continue;
				a = me_pc(me);
				if (a < watch || a >= whi)
					continue;
				words(me, w);
				if (hits++ < 50) {
					char tag[64];

					snprintf(tag, sizeof(tag), "hit me=%d pc=%u", me, a);
					print_words(tag, w, NULL);
					fflush(stdout);
				}
				latch(fill_inst);
				wr(PPU + 0xc0, 1U << me);
			}
		}
		printf("watch done hits=%u stopped_after=%x\n", hits, stopped());
		return 0;
	}
	gate = rd(GATE);
	wr(GATE, gate & ~8U);

	if (have_fill) {
		unsigned int a;

		if ((fill | dcount) & 3 || fill + dcount > 12288) {
			wr(GATE, gate);
			return 1;
		}
		for (a = fill; a < fill + dcount; a += 4) {
			for (i = 0; i < 4; i++)
				blk[i] = fill_inst;
			if (iblock(a, blk, 1)) {
				fprintf(stderr, "write failed at %u\n", a);
				ret = 2;
				break;
			}
		}
		printf("filled %u..%u\n", fill, a - 1);
		wr(GATE, gate);
		return ret;
	}
	if (have_dump) {
		unsigned int a;

		for (a = dump & ~3U; a < dump + dcount && a < 12288; a += 4) {
			if (iblock(a, blk, 0)) {
				ret = 2;
				break;
			}
			for (i = 0; i < 4; i++)
				printf("%u %016llx\n", a + i,
				       (unsigned long long)blk[i]);
		}
		wr(GATE, gate);
		return ret;
	}

	if (stopped()) {
		fprintf(stderr, "contexts already stopped: %x\n", stopped());
		wr(GATE, gate);
		return 3;
	}
	if (iblock(pc & ~3U, orig, 0)) {
		wr(GATE, gate);
		return 2;
	}
	memcpy(blk, orig, sizeof(blk));
	blk[pc & 3] = BKPT;
	latch(orig[pc & 3]);
	if (iblock(pc & ~3U, blk, 1)) {
		fprintf(stderr, "breakpoint write failed\n");
		iblock(pc & ~3U, orig, 1);
		wr(GATE, gate);
		return 2;
	}
	t0 = now_ms();
	while (sel < 0 && now_ms() - t0 < (long)timeout) {
		mask = stopped();
		for (me = 0; me < 8; me++) {
			if (!(mask & (1U << me)) || me_pc(me) != pc)
				continue;
			words(me, w);
			seen++;
			if (sel < 0 && matches(w)) {
				sel = me;
				continue;
			}
			if (seen <= 4)
				printf("skip me=%d w32=%08x w33=%08x w34=%08x w35=%08x w36=%08x w37=%08x\n",
				       me, w[32], w[33], w[34], w[35], w[36], w[37]);
			latch(orig[pc & 3]);
			wr(PPU + 0xc0, 1U << me);
		}
	}
	if (iblock(pc & ~3U, orig, 1)) {
		fprintf(stderr, "RESTORE FAILED\n");
		ret = 4;
	}
	/* Release every other context parked on the entry. */
	udelay_(1000);
	mask = stopped();
	for (me = 0; me < 8; me++)
		if ((mask & (1U << me)) && me != sel && me_pc(me) == pc) {
			latch(orig[pc & 3]);
			wr(PPU + 0xc0, 1U << me);
		}
	printf("pc=%u seen=%d selected=%d\n", pc, seen, sel);
	if (sel >= 0) {
		unsigned int n;
		uint32_t a = 0;

		for (n = 0; n < steps; n++) {
			if (!(stopped() & (1U << sel)))
				break;
			a = me_pc(sel);
			if (a >= 12288 || iblock(a & ~3U, blk, 0)) {
				ret = 5;
				break;
			}
			words(sel, w);
			{
				char tag[64];

				snprintf(tag, sizeof(tag), "step=%u pc=%u inst=%016llx",
					 n, a, (unsigned long long)blk[a & 3]);
				print_words(tag, w, n ? prev : NULL);
			}
			memcpy(prev, w, sizeof(prev));
			latch(blk[a & 3]);
			wr(PPU + 0xc4, 1U << sel);
			udelay_(200);
		}
		if (stopped() & (1U << sel)) {
			a = me_pc(sel);
			printf("still stopped at pc=%u after %u steps; resuming\n", a, n);
			if (a < 12288 && !iblock(a & ~3U, blk, 0)) {
				latch(blk[a & 3]);
				wr(PPU + 0xc0, 1U << sel);
			}
		} else {
			printf("context finished after %u steps\n", n);
		}
	}
	udelay_(1000);
	printf("stopped_after=%x\n", stopped());
	wr(GATE, gate);
	return ret;
}
