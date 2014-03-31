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

#ifndef _ASM_TILE_CACHEFLUSH_H
#define _ASM_TILE_CACHEFLUSH_H

#include <arch/chip.h>

#include <linux/mm.h>
#include <linux/cache.h>
#include <arch/icache.h>

#define flush_cache_all()			do { } while (0)
#define flush_cache_mm(mm)			do { } while (0)
#define flush_cache_dup_mm(mm)			do { } while (0)
#define flush_cache_range(vma, start, end)	do { } while (0)
#define flush_cache_page(vma, vmaddr, pfn)	do { } while (0)
#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 0
#define flush_dcache_page(page)			do { } while (0)
#define flush_dcache_mmap_lock(mapping)		do { } while (0)
#define flush_dcache_mmap_unlock(mapping)	do { } while (0)
#define flush_cache_vmap(start, end)		do { } while (0)
#define flush_cache_vunmap(start, end)		do { } while (0)
#define flush_icache_page(vma, pg)		do { } while (0)
#define flush_icache_user_range(vma, pg, adr, len)	do { } while (0)

extern void __flush_icache_range(unsigned long start, unsigned long end);

#define __flush_icache() __flush_icache_range(0, CHIP_L1I_CACHE_SIZE())

#ifdef CONFIG_SMP
extern void flush_icache_range(unsigned long start, unsigned long end);
#else
#define flush_icache_range __flush_icache_range
#endif

static inline void copy_to_user_page(struct vm_area_struct *vma,
				     struct page *page, unsigned long vaddr,
				     void *dst, void *src, int len)
{
	memcpy(dst, src, len);
	if (vma->vm_flags & VM_EXEC) {
		flush_icache_range((unsigned long) dst,
				   (unsigned long) dst + len);
	}
}

#define copy_from_user_page(vma, page, vaddr, dst, src, len) \
	memcpy((dst), (src), (len))

static inline void __inv_buffer(void *buffer, size_t size)
{
	char *next = (char *)((long)buffer & -L2_CACHE_BYTES);
	char *finish = (char *)L2_CACHE_ALIGN((long)buffer + size);
	while (next < finish) {
		__insn_inv(next);
		next += CHIP_INV_STRIDE();
	}
}

static inline void __flush_buffer(void *buffer, size_t size)
{
	char *next = (char *)((long)buffer & -L2_CACHE_BYTES);
	char *finish = (char *)L2_CACHE_ALIGN((long)buffer + size);
	while (next < finish) {
		__insn_flush(next);
		next += CHIP_FLUSH_STRIDE();
	}
}

static inline void __finv_buffer(void *buffer, size_t size)
{
	char *next = (char *)((long)buffer & -L2_CACHE_BYTES);
	char *finish = (char *)L2_CACHE_ALIGN((long)buffer + size);
	while (next < finish) {
		__insn_finv(next);
		next += CHIP_FINV_STRIDE();
	}
}


static inline void inv_buffer(void *buffer, size_t size)
{
	__inv_buffer(buffer, size);
	mb();
}

static inline void flush_buffer_local(void *buffer, size_t size)
{
	__flush_buffer(buffer, size);
	mb_incoherent();
}

static inline void finv_buffer_local(void *buffer, size_t size)
{
	__finv_buffer(buffer, size);
	mb_incoherent();
}

void finv_buffer_remote(void *buffer, size_t size, int hfh);

static inline void sched_cacheflush(void)
{
}

#endif 
