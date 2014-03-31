/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_AVR32_CACHEFLUSH_H
#define __ASM_AVR32_CACHEFLUSH_H

#include <linux/mm.h>

#define CACHE_OP_ICACHE_INVALIDATE	0x01
#define CACHE_OP_DCACHE_INVALIDATE	0x0b
#define CACHE_OP_DCACHE_CLEAN		0x0c
#define CACHE_OP_DCACHE_CLEAN_INVAL	0x0d

static inline void invalidate_dcache_line(void *vaddr)
{
	asm volatile("cache %0[0], %1"
		     :
		     : "r"(vaddr), "n"(CACHE_OP_DCACHE_INVALIDATE)
		     : "memory");
}

/*
 * Make sure any cacheline containing virtual address vaddr is written
 * to memory.
 */
static inline void clean_dcache_line(void *vaddr)
{
	asm volatile("cache %0[0], %1"
		     :
		     : "r"(vaddr), "n"(CACHE_OP_DCACHE_CLEAN)
		     : "memory");
}

/*
 * Make sure any cacheline containing virtual address vaddr is written
 * to memory and then invalidate it.
 */
static inline void flush_dcache_line(void *vaddr)
{
	asm volatile("cache %0[0], %1"
		     :
		     : "r"(vaddr), "n"(CACHE_OP_DCACHE_CLEAN_INVAL)
		     : "memory");
}

static inline void invalidate_icache_line(void *vaddr)
{
	asm volatile("cache %0[0], %1"
		     :
		     : "r"(vaddr), "n"(CACHE_OP_ICACHE_INVALIDATE)
		     : "memory");
}

void invalidate_dcache_region(void *start, size_t len);
void clean_dcache_region(void *start, size_t len);
void flush_dcache_region(void *start, size_t len);
void invalidate_icache_region(void *start, size_t len);

#define flush_write_buffer() asm volatile("sync 0" : : : "memory")

#define flush_cache_all()			do { } while (0)
#define flush_cache_mm(mm)			do { } while (0)
#define flush_cache_dup_mm(mm)			do { } while (0)
#define flush_cache_range(vma, start, end)	do { } while (0)
#define flush_cache_page(vma, vmaddr, pfn)	do { } while (0)
#define flush_cache_vmap(start, end)		do { } while (0)
#define flush_cache_vunmap(start, end)		do { } while (0)

extern void flush_icache_page(struct vm_area_struct *vma, struct page *page);

#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 0
#define flush_dcache_page(page)			do { } while (0)
#define flush_dcache_mmap_lock(page)		do { } while (0)
#define flush_dcache_mmap_unlock(page)		do { } while (0)

extern void flush_icache_range(unsigned long start, unsigned long end);

extern void copy_to_user_page(struct vm_area_struct *vma, struct page *page,
		unsigned long vaddr, void *dst, const void *src,
		unsigned long len);

static inline void copy_from_user_page(struct vm_area_struct *vma,
		struct page *page, unsigned long vaddr, void *dst,
		const void *src, unsigned long len)
{
	memcpy(dst, src, len);
}

#endif 
