/*
 * Copyright (C) 2007-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2007-2009 PetaLogix
 * Copyright (C) 2007 John Williams <john.williams@petalogix.com>
 * based on v850 version which was
 * Copyright (C) 2001,02,03 NEC Electronics Corporation
 * Copyright (C) 2001,02,03 Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file COPYING in the main directory of this
 * archive for more details.
 *
 */

#ifndef _ASM_MICROBLAZE_CACHEFLUSH_H
#define _ASM_MICROBLAZE_CACHEFLUSH_H

#include <linux/mm.h>
#include <linux/io.h>



struct scache {
	
	void (*ie)(void); 
	void (*id)(void); 
	void (*ifl)(void); 
	void (*iflr)(unsigned long a, unsigned long b);
	void (*iin)(void); 
	void (*iinr)(unsigned long a, unsigned long b);
	
	void (*de)(void); 
	void (*dd)(void); 
	void (*dfl)(void); 
	void (*dflr)(unsigned long a, unsigned long b);
	void (*din)(void); 
	void (*dinr)(unsigned long a, unsigned long b);
};

extern struct scache *mbc;

void microblaze_cache_init(void);

#define enable_icache()					mbc->ie();
#define disable_icache()				mbc->id();
#define flush_icache()					mbc->ifl();
#define flush_icache_range(start, end)			mbc->iflr(start, end);
#define invalidate_icache()				mbc->iin();
#define invalidate_icache_range(start, end)		mbc->iinr(start, end);

#define flush_icache_user_range(vma, pg, adr, len)	flush_icache();
#define flush_icache_page(vma, pg)			do { } while (0)

#define enable_dcache()					mbc->de();
#define disable_dcache()				mbc->dd();
#define invalidate_dcache()				mbc->din();
#define invalidate_dcache_range(start, end)		mbc->dinr(start, end);
#define flush_dcache()					mbc->dfl();
#define flush_dcache_range(start, end)			mbc->dflr(start, end);

#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 1
#define flush_dcache_page(page) \
do { \
	unsigned long addr = (unsigned long) page_address(page);  \
	addr = (u32)virt_to_phys((void *)addr); \
	flush_dcache_range((unsigned) (addr), (unsigned) (addr) + PAGE_SIZE); \
} while (0);

#define flush_dcache_mmap_lock(mapping)		do { } while (0)
#define flush_dcache_mmap_unlock(mapping)	do { } while (0)

#define flush_cache_dup_mm(mm)				do { } while (0)
#define flush_cache_vmap(start, end)			do { } while (0)
#define flush_cache_vunmap(start, end)			do { } while (0)
#define flush_cache_mm(mm)			do { } while (0)

#define flush_cache_page(vma, vmaddr, pfn) \
	flush_dcache_range(pfn << PAGE_SHIFT, (pfn << PAGE_SHIFT) + PAGE_SIZE);

#if 0
#define flush_cache_range(vma, start, len)	{	\
	flush_icache_range((unsigned) (start), (unsigned) (start) + (len)); \
	flush_dcache_range((unsigned) (start), (unsigned) (start) + (len)); \
}
#endif

#define flush_cache_range(vma, start, len) do { } while (0)

#define copy_to_user_page(vma, page, vaddr, dst, src, len)		\
do {									\
	u32 addr = virt_to_phys(dst);					\
	memcpy((dst), (src), (len));					\
	if (vma->vm_flags & VM_EXEC) {					\
		invalidate_icache_range((unsigned) (addr),		\
					(unsigned) (addr) + PAGE_SIZE);	\
		flush_dcache_range((unsigned) (addr),			\
					(unsigned) (addr) + PAGE_SIZE);	\
	}								\
} while (0)

#define copy_from_user_page(vma, page, vaddr, dst, src, len)		\
do {									\
	memcpy((dst), (src), (len));					\
} while (0)

#endif 
