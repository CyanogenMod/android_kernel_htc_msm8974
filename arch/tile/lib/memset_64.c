/*
 * Copyright 2011 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#include <arch/chip.h>

#include <linux/types.h>
#include <linux/string.h>
#include <linux/module.h>

#undef memset

void *memset(void *s, int c, size_t n)
{
	uint64_t *out64;
	int n64, to_align64;
	uint64_t v64;
	uint8_t *out8 = s;

#define BYTE_CUTOFF 20

#if BYTE_CUTOFF < 7
#error "BYTE_CUTOFF is too small"
#endif

	if (n < BYTE_CUTOFF) {
		if (n != 0) {
			do {
				*out8 = c;
				out8++;
			} while (--n != 0);
		}

		return s;
	}

	
	while (((uintptr_t) out8 & 7) != 0) {
		*out8++ = c;
		--n;
	}

	
	while (n & 7)
		out8[--n] = c;

	out64 = (uint64_t *) out8;
	n64 = n >> 3;

	
	
	v64 = 0x0101010101010101ULL * (uint8_t)c;

	
#define CACHE_LINE_SIZE_IN_DOUBLEWORDS (CHIP_L2_LINE_SIZE() / 8)

	to_align64 = (-((uintptr_t)out64 >> 3)) &
		(CACHE_LINE_SIZE_IN_DOUBLEWORDS - 1);

	if (to_align64 <= n64 - CACHE_LINE_SIZE_IN_DOUBLEWORDS) {
		int lines_left;

		
		n64 -= to_align64;
		for (; to_align64 != 0; to_align64--) {
			*out64 = v64;
			out64++;
		}

		
		lines_left = (unsigned)n64 / CACHE_LINE_SIZE_IN_DOUBLEWORDS;

		do {
			int x = ((lines_left < CHIP_MAX_OUTSTANDING_VICTIMS())
				  ? lines_left
				  : CHIP_MAX_OUTSTANDING_VICTIMS());
			uint64_t *wh = out64;
			int i = x;
			int j;

			lines_left -= x;

			do {
				__insn_wh64(wh);
				wh += CACHE_LINE_SIZE_IN_DOUBLEWORDS;
			} while (--i);

			for (j = x * (CACHE_LINE_SIZE_IN_DOUBLEWORDS / 4);
			     j != 0; j--) {
				*out64++ = v64;
				*out64++ = v64;
				*out64++ = v64;
				*out64++ = v64;
			}
		} while (lines_left != 0);

		n64 &= CACHE_LINE_SIZE_IN_DOUBLEWORDS - 1;
	}

	
	if (n64 != 0) {
		do {
			*out64 = v64;
			out64++;
		} while (--n64 != 0);
	}

	return s;
}
EXPORT_SYMBOL(memset);
