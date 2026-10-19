// SPDX-License-Identifier: GPL-2.0-only
/*
 * zx-slot - show and select the SR1010 firmware slot, from either system.
 *
 * The vendor bootloader starts the valid slot carrying the highest serial
 * number. This tool only reads the two slots and, for "switch", rewrites the
 * version header of the chosen slot with a serial above every other one. It
 * never writes a package: use fw_flashing (vendor) or zte-fwslot (OpenWrt).
 *
 * Unlike zte-fwslot it does not need the MTD "offset" attribute in sysfs,
 * which the vendor 5.4 kernel does not have. The slots sit at fixed flash
 * addresses; the device is found by name in /proc/mtd:
 *   OpenWrt:  "firmware-slot1" and "firmware-slot2", one partition per slot
 *   vendor:   "whole flash", the slots at 0x600000 and 0x2f00000 inside it
 *
 * The header format, the CRCs and the slot check follow zte-fwslot.c.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <mtd/mtd-user.h>

#define VER_HDR_SIZE	0x200
#define UBOOT_SIZE	0x180000
#define KERNEL_MAGIC_SIZE 32	/* in front of the kernel, behind the U-Boot */

#define H_VERSION	0x10
#define H_FW_SIZE	0x30
#define H_KERNEL_SIZE	0x34
#define H_KERNEL_FILEOFS 0x38
#define H_KERNEL_CRC	0x3c
#define H_FS_SIZE	0x40
#define H_FS_FILEOFS	0x44
#define H_FS_CRC	0x48
#define H_UBOOT_CRC	0x98
#define H_CRC		0xa4
#define H_INNER_MAGIC	0xf4
#define H_SERIAL	0x1f4
#define H_CRC2		0x1fc

#define SLOT_SIZE	0x2900000

static const uint8_t inner_magic[16] = {
	0x33,0x33,0x33,0x33, 0x66,0x66,0x66,0x66,
	0x99,0x99,0x99,0x99, 0xcc,0xcc,0xcc,0xcc,
};

/* The two firmware slots, by absolute offset in the whole flash. */
static const uint32_t slot_start[2] = { 0x600000, 0x2f00000 };

/* One slot: an MTD device and where the slot starts inside it. */
struct slot {
	int fd;
	uint64_t base;
	uint32_t bs;		/* erase block size */
	char dev[24];
	bool valid;		/* a version header was found */
	unsigned int bad;	/* bad blocks skipped while verifying */
	uint64_t hpos;		/* header offset within the slot */
	uint8_t hdr[VER_HDR_SIZE];
};

static uint32_t crc_table[256];

static void crc_init(void)
{
	for (uint32_t i = 0; i < 256; i++) {
		uint32_t c = i;

		for (int k = 0; k < 8; k++)
			c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
		crc_table[i] = c;
	}
}

static uint32_t crc32_update(uint32_t c, const void *buf, size_t len)
{
	const uint8_t *p = buf;

	while (len--)
		c = crc_table[(c ^ *p++) & 0xff] ^ (c >> 8);
	return c;
}

static uint32_t crc32(const void *buf, size_t len)
{
	return crc32_update(0xffffffffu, buf, len) ^ 0xffffffffu;
}

static uint32_t rd32(const uint8_t *p, size_t ofs)
{
	return p[ofs] | p[ofs + 1] << 8 | p[ofs + 2] << 16 |
	       (uint32_t)p[ofs + 3] << 24;
}

static void wr32(uint8_t *p, size_t ofs, uint32_t v)
{
	p[ofs] = v; p[ofs + 1] = v >> 8;
	p[ofs + 2] = v >> 16; p[ofs + 3] = v >> 24;
}

static void die(const char *msg)
{
	fprintf(stderr, "zx-slot: %s: %s\n", msg, strerror(errno));
	exit(1);
}

/* The index of the MTD device with this name in /proc/mtd, or -1. */
static int mtd_by_name(const char *name)
{
	FILE *f = fopen("/proc/mtd", "r");
	char line[128], want[64];
	int idx, found = -1;

	if (!f)
		die("/proc/mtd");
	snprintf(want, sizeof(want), "\"%s\"", name);
	while (fgets(line, sizeof(line), f))
		if (strstr(line, want) && sscanf(line, "mtd%d:", &idx) == 1) {
			found = idx;
			break;
		}
	fclose(f);
	return found;
}

static void slot_open(struct slot *s, int n, bool writable)
{
	static const char *const part[2] = { "firmware-slot1", "firmware-slot2" };
	struct mtd_info_user info;
	int idx = mtd_by_name(part[n]);

	s->base = 0;
	if (idx < 0) {
		idx = mtd_by_name("whole flash");
		s->base = slot_start[n];
	}
	if (idx < 0)
		errno = ENODEV, die("no firmware-slot or whole-flash MTD device");
	snprintf(s->dev, sizeof(s->dev), "/dev/mtd%d", idx);
	s->fd = open(s->dev, writable ? O_RDWR : O_RDONLY);
	if (s->fd < 0)
		die(s->dev);
	if (ioctl(s->fd, MEMGETINFO, &info) < 0)
		die("MEMGETINFO");
	if (s->base + SLOT_SIZE > info.size)
		errno = EINVAL, die("MTD device smaller than the slot");
	s->bs = info.erasesize;
}

static bool slot_bad(struct slot *s, uint64_t ofs)
{
	long long o = s->base + ofs;

	return ioctl(s->fd, MEMGETBADBLOCK, &o) > 0;
}

static int slot_pread(struct slot *s, void *buf, size_t len, uint64_t ofs)
{
	return pread(s->fd, buf, len, s->base + ofs) == (ssize_t)len ? 0 : -1;
}

/*
 * Find the version header: the first good block carrying the inner magic at
 * +0xf4 and a CRC over [0:0xa4] at +0xa4.
 */
static void slot_scan(struct slot *s)
{
	s->valid = false;
	for (uint64_t ofs = 0; ofs + VER_HDR_SIZE <= SLOT_SIZE; ofs += s->bs) {
		if (slot_bad(s, ofs))
			continue;
		if (slot_pread(s, s->hdr, VER_HDR_SIZE, ofs))
			die("read");
		if (memcmp(s->hdr + H_INNER_MAGIC, inner_magic, 16) ||
		    crc32(s->hdr, H_CRC) != rd32(s->hdr, H_CRC))
			continue;
		s->valid = true;
		s->hpos = ofs;
		return;
	}
}

struct region {
	const char *fail;
	uint64_t start, len;
	uint32_t want, crc;
};

static void region_feed(struct region *r, uint64_t pos, const uint8_t *buf,
			size_t n)
{
	uint64_t lo = pos > r->start ? pos : r->start;
	uint64_t end = r->start + r->len;
	uint64_t hi = pos + n < end ? pos + n : end;

	if (lo < hi)
		r->crc = crc32_update(r->crc, buf + (lo - pos), hi - lo);
}

/*
 * Check a slot as the bootloader goes by it: the header, then the U-Boot, the
 * kernel and the root filesystem, read from the good blocks in order.
 * Returns NULL for an intact slot, otherwise what is wrong with it.
 */
static const char *slot_verify(struct slot *s)
{
	const uint8_t *hdr = s->hdr;
	uint64_t body_len = rd32(hdr, H_FW_SIZE);
	uint32_t koff = rd32(hdr, H_KERNEL_FILEOFS);
	uint32_t foff = rd32(hdr, H_FS_FILEOFS);
	uint64_t kstart = UBOOT_SIZE + KERNEL_MAGIC_SIZE;
	uint64_t pos = 0, ofs = 0;
	const char *bad = NULL;
	struct region r[3];
	uint8_t *buf;

	if (crc32(hdr, H_CRC2) != rd32(hdr, H_CRC2))
		return "version header CRC mismatch";
	if (foff < koff || body_len > SLOT_SIZE)
		return "bad firmware layout";

	r[0] = (struct region){ "U-Boot CRC mismatch", 0, UBOOT_SIZE,
				rd32(hdr, H_UBOOT_CRC), 0xffffffffu };
	r[1] = (struct region){ "kernel CRC mismatch", kstart,
				rd32(hdr, H_KERNEL_SIZE),
				rd32(hdr, H_KERNEL_CRC), 0xffffffffu };
	r[2] = (struct region){ "root filesystem CRC mismatch",
				kstart + (foff - koff), rd32(hdr, H_FS_SIZE),
				rd32(hdr, H_FS_CRC), 0xffffffffu };
	for (int i = 0; i < 3; i++)
		if (r[i].start + r[i].len > body_len)
			return "bad firmware layout";

	buf = malloc(s->bs);
	if (!buf)
		return "out of memory";
	while (pos < body_len) {
		size_t n = body_len - pos < s->bs ? body_len - pos : s->bs;

		if (ofs + s->bs > SLOT_SIZE) {
			bad = "payload runs past the end of the slot";
			break;
		}
		if (slot_bad(s, ofs)) {
			s->bad++;
			ofs += s->bs;
			continue;
		}
		if (slot_pread(s, buf, n, ofs)) {
			bad = "read error";
			break;
		}
		for (int i = 0; i < 3; i++)
			region_feed(&r[i], pos, buf, n);
		pos += n;
		ofs += s->bs;
	}
	free(buf);
	if (bad)
		return bad;
	for (int i = 0; i < 3; i++)
		if ((r[i].crc ^ 0xffffffffu) != r[i].want)
			return r[i].fail;
	return NULL;
}

/*
 * Through "whole flash" the vendor 5.4 kernel reads part of the slots wrong:
 * on 2026-10-15 a block holding a valid header read back as all 0xff, and
 * root filesystem CRCs failed that pass from OpenWrt. A switch that followed
 * such a read wrote the new serial into a stale header left from an older
 * firmware. Never write from there.
 */
static bool slots_trustworthy(struct slot *s)
{
	return s->base == 0;
}

/* The slot the bootloader starts next: valid, with the highest serial. */
static int next_slot(struct slot *s)
{
	int best = -1;

	for (int n = 0; n < 2; n++)
		if (s[n].valid && (best < 0 ||
		    rd32(s[n].hdr, H_SERIAL) > rd32(s[best].hdr, H_SERIAL)))
			best = n;
	return best;
}

static int cmd_info(void)
{
	struct slot s[2];
	int next;

	for (int n = 0; n < 2; n++) {
		slot_open(&s[n], n, false);
		slot_scan(&s[n]);
	}
	next = next_slot(s);
	if (!slots_trustworthy(&s[0]))
		printf("note: read through the vendor kernel, which reads the slots unreliably\n");
	for (int n = 0; n < 2; n++) {
		printf("slot %d: start %#x %s+%#llx", n, slot_start[n], s[n].dev,
		       (unsigned long long)s[n].base);
		if (!s[n].valid)
			printf(" (no valid firmware)\n");
		else
			printf(" header@%#llx serial=%u version=\"%.32s\"%s\n",
			       (unsigned long long)s[n].hpos,
			       rd32(s[n].hdr, H_SERIAL),
			       (const char *)s[n].hdr + H_VERSION,
			       n == next ? " [next boot]" : "");
		close(s[n].fd);
	}
	return 0;
}

static int cmd_check(int target)
{
	struct slot s;
	const char *why;

	slot_open(&s, target, false);
	slot_scan(&s);
	if (!s.valid) {
		fprintf(stderr, "zx-slot: slot %d holds no firmware\n", target);
		return 1;
	}
	why = slot_verify(&s);
	close(s.fd);
	if (why) {
		fprintf(stderr, "zx-slot: slot %d is damaged (%s), %u bad blocks skipped\n",
			target, why, s.bad);
		return 1;
	}
	printf("ok: slot %d is intact (serial %u), %u bad blocks skipped\n",
	       target, rd32(s.hdr, H_SERIAL), s.bad);
	return 0;
}

static int cmd_switch(int target)
{
	struct slot s[2];
	uint32_t serial = 0;
	const char *why;
	uint8_t *blk;

	for (int n = 0; n < 2; n++) {
		slot_open(&s[n], n, n == target);
		if (!slots_trustworthy(&s[n])) {
			fprintf(stderr,
				"zx-slot: the vendor kernel reads the slots unreliably; not switching.\n"
				"         Write the wanted firmware again with: fw_flashing -f <package>\n");
			return 1;
		}
		slot_scan(&s[n]);
		if (s[n].valid && rd32(s[n].hdr, H_SERIAL) > serial)
			serial = rd32(s[n].hdr, H_SERIAL);
	}
	if (!s[target].valid) {
		fprintf(stderr, "zx-slot: slot %d holds no firmware\n", target);
		return 1;
	}
	if (next_slot(s) == target) {
		fprintf(stderr, "zx-slot: slot %d already boots next\n", target);
		return 0;
	}
	/* The bootloader does not fall back: never point it at a broken slot. */
	why = slot_verify(&s[target]);
	if (why) {
		fprintf(stderr, "zx-slot: slot %d is damaged (%s), not switching\n",
			target, why);
		return 1;
	}

	wr32(s[target].hdr, H_SERIAL, serial + 1);
	wr32(s[target].hdr, H_CRC2, crc32(s[target].hdr, H_CRC2));

	/* NAND takes whole pages only: rewrite the full block, padded with 0xff. */
	blk = malloc(s[target].bs);
	if (!blk)
		die("malloc");
	memset(blk, 0xff, s[target].bs);
	memcpy(blk, s[target].hdr, VER_HDR_SIZE);
	{
		struct erase_info_user e = {
			.start = s[target].base + s[target].hpos,
			.length = s[target].bs,
		};

		ioctl(s[target].fd, MEMUNLOCK, &e);
		if (ioctl(s[target].fd, MEMERASE, &e) < 0)
			die("erase header block");
	}
	if (pwrite(s[target].fd, blk, s[target].bs,
		   s[target].base + s[target].hpos) != (ssize_t)s[target].bs)
		die("write header");
	free(blk);

	/* Read it back: the bootloader must find what was just written. */
	slot_scan(&s[target]);
	if (!s[target].valid || rd32(s[target].hdr, H_SERIAL) != serial + 1 ||
	    crc32(s[target].hdr, H_CRC2) != rd32(s[target].hdr, H_CRC2)) {
		fprintf(stderr, "zx-slot: header read-back failed\n");
		return 1;
	}
	for (int n = 0; n < 2; n++)
		close(s[n].fd);
	printf("ok: next boot selects slot %d (serial %u)\n", target, serial + 1);
	return 0;
}

int main(int argc, char **argv)
{
	crc_init();
	if (argc == 2 && !strcmp(argv[1], "info"))
		return cmd_info();
	if (argc == 3 && (!strcmp(argv[2], "0") || !strcmp(argv[2], "1"))) {
		if (!strcmp(argv[1], "switch"))
			return cmd_switch(argv[2][0] - '0');
		if (!strcmp(argv[1], "check"))
			return cmd_check(argv[2][0] - '0');
	}

	fprintf(stderr,
		"usage: zx-slot info          show both slots and which one boots next\n"
		"       zx-slot check 0|1     verify the CRCs of that slot, read only\n"
		"       zx-slot switch 0|1    check that slot and make it boot next (OpenWrt only)\n");
	return 1;
}
