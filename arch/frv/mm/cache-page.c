/* cache-page.c: whole-page cache wrangling functions for MMU linux
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <asm/pgalloc.h>

void flush_dcache_page(struct page *page)
{
	unsigned long dampr2;
	void *vaddr;

	dampr2 = __get_DAMPR(2);

	vaddr = kmap_atomic_primary(page, __KM_CACHE);

	frv_dcache_writeback((unsigned long) vaddr, (unsigned long) vaddr + PAGE_SIZE);

	kunmap_atomic_primary(vaddr, __KM_CACHE);

	if (dampr2) {
		__set_DAMPR(2, dampr2);
		__set_IAMPR(2, dampr2);
	}

} 

EXPORT_SYMBOL(flush_dcache_page);

void flush_icache_user_range(struct vm_area_struct *vma, struct page *page,
			     unsigned long start, unsigned long len)
{
	unsigned long dampr2;
	void *vaddr;

	dampr2 = __get_DAMPR(2);

	vaddr = kmap_atomic_primary(page, __KM_CACHE);

	start = (start & ~PAGE_MASK) | (unsigned long) vaddr;
	frv_cache_wback_inv(start, start + len);

	kunmap_atomic_primary(vaddr, __KM_CACHE);

	if (dampr2) {
		__set_DAMPR(2, dampr2);
		__set_IAMPR(2, dampr2);
	}

} 

EXPORT_SYMBOL(flush_icache_user_range);
