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

#include <asm/page.h>
#include <asm/cacheflush.h>
#include <arch/icache.h>
#include <arch/spr_def.h>


void __flush_icache_range(unsigned long start, unsigned long end)
{
	invalidate_icache((const void *)start, end - start, PAGE_SIZE);
}


static inline void force_load(char *p)
{
	*(volatile char *)p;
}

void finv_buffer_remote(void *buffer, size_t size, int hfh)
{
	char *p, *base;
	size_t step_size, load_count;

#ifdef __tilegx__
	const unsigned long STRIPE_WIDTH = 512;
#else
	const unsigned long STRIPE_WIDTH = 8192;
#endif

#ifdef __tilegx__
	uint_reg_t old_dstream_pf = __insn_mfspr(SPR_DSTREAM_PF);
	__insn_mtspr(SPR_DSTREAM_PF, 0);
#endif

	__finv_buffer(buffer, size);

	__insn_mf();

	if (hfh) {
		step_size = L2_CACHE_BYTES;
#ifdef __tilegx__
		load_count = (size + L2_CACHE_BYTES - 1) / L2_CACHE_BYTES;
#else
		load_count = (STRIPE_WIDTH / L2_CACHE_BYTES) *
			      (1 << CHIP_LOG_NUM_MSHIMS());
#endif
	} else {
		step_size = STRIPE_WIDTH;
		load_count = (1 << CHIP_LOG_NUM_MSHIMS());
	}

	
	p = (char *)buffer + size - 1;
	force_load(p);

	
	p -= step_size;
	p = (char *)((unsigned long)p | (step_size - 1));

	
	base = p - (step_size * (load_count - 2));
	if ((unsigned long)base < (unsigned long)buffer)
		base = buffer;

#pragma unroll 8
	for (; p >= base; p -= step_size)
		force_load(p);

	p = (char *)buffer + size - 1;
	__insn_inv(p);
	p -= step_size;
	p = (char *)((unsigned long)p | (step_size - 1));
	for (; p >= base; p -= step_size)
		__insn_inv(p);

	
	__insn_mf();

#ifdef __tilegx__
	
	__insn_mtspr(SPR_DSTREAM_PF, old_dstream_pf);
#endif
}
