/*
 * vrl4 format generator
 *
 * Copyright (C) 2010 Simon Horman
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */


#define _BSD_SOURCE
#include <endian.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

struct hdr {
	uint32_t magic1;
	uint32_t reserved1;
	uint32_t magic2;
	uint32_t reserved2;
	uint16_t copy_size;
	uint16_t boot_options;
	uint32_t reserved3;
	uint32_t start_address;
	uint32_t reserved4;
	uint32_t reserved5;
	char     reserved6[308];
};

#define DECLARE_HDR(h)					\
	struct hdr (h) = {				\
		.magic1 =	htole32(0xea000000),	\
		.reserved1 =	htole32(0x56),		\
		.magic2 =	htole32(0xe59ff008),	\
		.reserved3 =	htole16(0x1) }

#define ALIGN_BITS	9
#define ALIGN		(1 << ALIGN_BITS)

#define START_BASE	0xe55b0000

#define	MAX_BOOT_PROG_LEN (16 * 512)

#define ROUND_UP(x)	((x + ALIGN - 1) & ~(ALIGN - 1))

ssize_t do_read(int fd, void *buf, size_t count)
{
	size_t offset = 0;
	ssize_t l;

	while (offset < count) {
		l = read(fd, buf + offset, count - offset);
		if (!l)
			break;
		if (l < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			perror("read");
			return -1;
		}
		offset += l;
	}

	return offset;
}

ssize_t do_write(int fd, const void *buf, size_t count)
{
	size_t offset = 0;
	ssize_t l;

	while (offset < count) {
		l = write(fd, buf + offset, count - offset);
		if (l < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			perror("write");
			return -1;
		}
		offset += l;
	}

	return offset;
}

ssize_t write_zero(int fd, size_t len)
{
	size_t i = len;

	while (i--) {
		const char x = 0;
		if (do_write(fd, &x, 1) < 0)
			return -1;
	}

	return len;
}

int main(void)
{
	DECLARE_HDR(hdr);
	char boot_program[MAX_BOOT_PROG_LEN];
	size_t aligned_hdr_len, alligned_prog_len;
	ssize_t prog_len;

	prog_len = do_read(0, boot_program, sizeof(boot_program));
	if (prog_len <= 0)
		return -1;

	aligned_hdr_len = ROUND_UP(sizeof(hdr));
	hdr.start_address = htole32(START_BASE + aligned_hdr_len);
	alligned_prog_len = ROUND_UP(prog_len);
	hdr.copy_size = htole16(aligned_hdr_len + alligned_prog_len);

	if (do_write(1, &hdr, sizeof(hdr)) < 0)
		return -1;
	if (write_zero(1, aligned_hdr_len - sizeof(hdr)) < 0)
		return -1;

	if (do_write(1, boot_program, prog_len) < 0)
		return 1;

	
	while (1) {
		prog_len = do_read(0, boot_program, sizeof(boot_program));
		if (prog_len < 0)
			return 1;
		if (prog_len == 0)
			break;
		if (do_write(1, boot_program, prog_len) < 0)
			return 1;
	}

	return 0;
}
