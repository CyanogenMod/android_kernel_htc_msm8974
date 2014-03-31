/*
 * Cache flush operations for the Hexagon architecture
 *
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _ASM_CACHEFLUSH_H
#define _ASM_CACHEFLUSH_H

#include <linux/cache.h>
#include <linux/mm.h>
#include <asm/string.h>
#include <asm-generic/cacheflush.h>

#define LINESIZE	32
#define LINEBITS	5

extern void flush_dcache_range(unsigned long start, unsigned long end);

#undef flush_icache_range
extern void flush_icache_range(unsigned long start, unsigned long end);

extern void flush_cache_all_hexagon(void);

static inline void update_mmu_cache(struct vm_area_struct *vma,
					unsigned long address, pte_t *ptep)
{
	
}

#undef copy_to_user_page
static inline void copy_to_user_page(struct vm_area_struct *vma,
					     struct page *page,
					     unsigned long vaddr,
					     void *dst, void *src, int len)
{
	memcpy(dst, src, len);
	if (vma->vm_flags & VM_EXEC) {
		flush_icache_range((unsigned long) dst,
		(unsigned long) dst + len);
	}
}


extern void hexagon_inv_dcache_range(unsigned long start, unsigned long end);
extern void hexagon_clean_dcache_range(unsigned long start, unsigned long end);

#endif
