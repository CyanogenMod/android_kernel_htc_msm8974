/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
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
	uint32_t *out32;
	int n32;
	uint32_t v16, v32;
	uint8_t *out8 = s;
#if !CHIP_HAS_WH64()
	int ahead32;
#else
	int to_align32;
#endif

#define BYTE_CUTOFF 20

#if BYTE_CUTOFF < 3
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

#if !CHIP_HAS_WH64()
	__insn_prefetch(out8);

	__insn_prefetch(&out8[n - 1]);
#endif 


	
	while (((uintptr_t) out8 & 3) != 0) {
		*out8++ = c;
		--n;
	}

	
	while (n & 3)
		out8[--n] = c;

	out32 = (uint32_t *) out8;
	n32 = n >> 2;

	
	v16 = __insn_intlb(c, c);
	v32 = __insn_intlh(v16, v16);

	
#define CACHE_LINE_SIZE_IN_WORDS (CHIP_L2_LINE_SIZE() / 4)

#if !CHIP_HAS_WH64()

	ahead32 = CACHE_LINE_SIZE_IN_WORDS;

	if (n32 > CACHE_LINE_SIZE_IN_WORDS * 2) {
		int i;

#define MAX_PREFETCH 5
		ahead32 = n32 & -CACHE_LINE_SIZE_IN_WORDS;
		if (ahead32 > MAX_PREFETCH * CACHE_LINE_SIZE_IN_WORDS)
			ahead32 = MAX_PREFETCH * CACHE_LINE_SIZE_IN_WORDS;

		for (i = CACHE_LINE_SIZE_IN_WORDS;
		     i < ahead32; i += CACHE_LINE_SIZE_IN_WORDS)
			__insn_prefetch(&out32[i]);
	}

	if (n32 > ahead32) {
		while (1) {
			int j;

			__insn_prefetch(&out32[ahead32]);

#if CACHE_LINE_SIZE_IN_WORDS % 4 != 0
#error "Unhandled CACHE_LINE_SIZE_IN_WORDS"
#endif

			n32 -= CACHE_LINE_SIZE_IN_WORDS;

			for (j = CACHE_LINE_SIZE_IN_WORDS / 4; j > 0; j--) {
				*out32++ = v32;
				*out32++ = v32;
				*out32++ = v32;
				*out32++ = v32;
			}

			if (n32 <= ahead32) {
				if (n32 < CACHE_LINE_SIZE_IN_WORDS)
					break;

				ahead32 = CACHE_LINE_SIZE_IN_WORDS - 1;
			}
		}
	}

#else 

	to_align32 =
		(-((uintptr_t)out32 >> 2)) & (CACHE_LINE_SIZE_IN_WORDS - 1);

	if (to_align32 <= n32 - CACHE_LINE_SIZE_IN_WORDS) {
		int lines_left;

		
		n32 -= to_align32;
		for (; to_align32 != 0; to_align32--) {
			*out32 = v32;
			out32++;
		}

		
		lines_left = (unsigned)n32 / CACHE_LINE_SIZE_IN_WORDS;

		do {
			int x = ((lines_left < CHIP_MAX_OUTSTANDING_VICTIMS())
				  ? lines_left
				  : CHIP_MAX_OUTSTANDING_VICTIMS());
			uint32_t *wh = out32;
			int i = x;
			int j;

			lines_left -= x;

			do {
				__insn_wh64(wh);
				wh += CACHE_LINE_SIZE_IN_WORDS;
			} while (--i);

			for (j = x * (CACHE_LINE_SIZE_IN_WORDS / 4);
			     j != 0; j--) {
				*out32++ = v32;
				*out32++ = v32;
				*out32++ = v32;
				*out32++ = v32;
			}
		} while (lines_left != 0);

		n32 &= CACHE_LINE_SIZE_IN_WORDS - 1;
	}

#endif 

	
	if (n32 != 0) {
		do {
			*out32 = v32;
			out32++;
		} while (--n32 != 0);
	}

	return s;
}
EXPORT_SYMBOL(memset);
