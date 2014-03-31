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

#include <linux/types.h>
#include <linux/string.h>
#include <linux/module.h>

#undef strchr

char *strchr(const char *s, int c)
{
	int z, g;

	
	const uintptr_t s_int = (uintptr_t) s;
	const uint64_t *p = (const uint64_t *)(s_int & -8);

	
	const uint64_t goal = 0x0101010101010101ULL * (uint8_t) c;

	const uint64_t before_mask = (1ULL << (s_int << 3)) - 1;
	uint64_t v = (*p | before_mask) ^
		(goal & __insn_v1shrsi(before_mask, 1));

	uint64_t zero_matches, goal_matches;
	while (1) {
		
		zero_matches = __insn_v1cmpeqi(v, 0);

		
		goal_matches = __insn_v1cmpeq(v, goal);

		if (__builtin_expect((zero_matches | goal_matches) != 0, 0))
			break;

		v = *++p;
	}

	z = __insn_ctz(zero_matches);
	g = __insn_ctz(goal_matches);

	return (g <= z) ? ((char *)p) + (g >> 3) : NULL;
}
EXPORT_SYMBOL(strchr);
