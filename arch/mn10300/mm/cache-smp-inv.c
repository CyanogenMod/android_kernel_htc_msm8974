/* Functions for global i/dcache invalidation when caching in SMP
 *
 * Copyright (C) 2010 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#include <linux/mm.h>
#include <asm/cacheflush.h>
#include "cache-smp.h"

void mn10300_icache_inv(void)
{
	unsigned long flags;

	flags = smp_lock_cache();
	mn10300_local_icache_inv();
	smp_cache_call(SMP_ICACHE_INV, 0, 0);
	smp_unlock_cache(flags);
}

void mn10300_icache_inv_page(unsigned long start)
{
	unsigned long flags;

	start &= ~(PAGE_SIZE-1);

	flags = smp_lock_cache();
	mn10300_local_icache_inv_page(start);
	smp_cache_call(SMP_ICACHE_INV_RANGE, start, start + PAGE_SIZE);
	smp_unlock_cache(flags);
}

void mn10300_icache_inv_range(unsigned long start, unsigned long end)
{
	unsigned long flags;

	flags = smp_lock_cache();
	mn10300_local_icache_inv_range(start, end);
	smp_cache_call(SMP_ICACHE_INV_RANGE, start, end);
	smp_unlock_cache(flags);
}

void mn10300_icache_inv_range2(unsigned long start, unsigned long size)
{
	unsigned long flags;

	flags = smp_lock_cache();
	mn10300_local_icache_inv_range2(start, size);
	smp_cache_call(SMP_ICACHE_INV_RANGE, start, start + size);
	smp_unlock_cache(flags);
}

void mn10300_dcache_inv(void)
{
	unsigned long flags;

	flags = smp_lock_cache();
	mn10300_local_dcache_inv();
	smp_cache_call(SMP_DCACHE_INV, 0, 0);
	smp_unlock_cache(flags);
}

void mn10300_dcache_inv_page(unsigned long start)
{
	unsigned long flags;

	start &= ~(PAGE_SIZE-1);

	flags = smp_lock_cache();
	mn10300_local_dcache_inv_page(start);
	smp_cache_call(SMP_DCACHE_INV_RANGE, start, start + PAGE_SIZE);
	smp_unlock_cache(flags);
}

void mn10300_dcache_inv_range(unsigned long start, unsigned long end)
{
	unsigned long flags;

	flags = smp_lock_cache();
	mn10300_local_dcache_inv_range(start, end);
	smp_cache_call(SMP_DCACHE_INV_RANGE, start, end);
	smp_unlock_cache(flags);
}

void mn10300_dcache_inv_range2(unsigned long start, unsigned long size)
{
	unsigned long flags;

	flags = smp_lock_cache();
	mn10300_local_dcache_inv_range2(start, size);
	smp_cache_call(SMP_DCACHE_INV_RANGE, start, start + size);
	smp_unlock_cache(flags);
}
