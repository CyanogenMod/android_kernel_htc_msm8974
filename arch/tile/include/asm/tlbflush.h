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

#ifndef _ASM_TILE_TLBFLUSH_H
#define _ASM_TILE_TLBFLUSH_H

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <asm/cacheflush.h>
#include <asm/page.h>
#include <hv/hypervisor.h>

DECLARE_PER_CPU(int, current_asid);

extern int min_asid, max_asid;

static inline unsigned long hv_page_size(const struct vm_area_struct *vma)
{
	return (vma->vm_flags & VM_HUGETLB) ? HPAGE_SIZE : PAGE_SIZE;
}

#define FLUSH_NONEXEC ((const struct vm_area_struct *)-1UL)

static inline void local_flush_tlb_page(const struct vm_area_struct *vma,
					unsigned long addr,
					unsigned long page_size)
{
	int rc = hv_flush_page(addr, page_size);
	if (rc < 0)
		panic("hv_flush_page(%#lx,%#lx) failed: %d",
		      addr, page_size, rc);
	if (!vma || (vma != FLUSH_NONEXEC && (vma->vm_flags & VM_EXEC)))
		__flush_icache();
}

static inline void local_flush_tlb_pages(const struct vm_area_struct *vma,
					 unsigned long addr,
					 unsigned long page_size,
					 unsigned long len)
{
	int rc = hv_flush_pages(addr, page_size, len);
	if (rc < 0)
		panic("hv_flush_pages(%#lx,%#lx,%#lx) failed: %d",
		      addr, page_size, len, rc);
	if (!vma || (vma != FLUSH_NONEXEC && (vma->vm_flags & VM_EXEC)))
		__flush_icache();
}

static inline void local_flush_tlb(void)
{
	int rc = hv_flush_all(1);   
	if (rc < 0)
		panic("hv_flush_all(1) failed: %d", rc);
	__flush_icache();
}

static inline void local_flush_tlb_all(void)
{
	int i;
	for (i = 0; ; ++i) {
		HV_VirtAddrRange r = hv_inquire_virtual(i);
		if (r.size == 0)
			break;
		local_flush_tlb_pages(NULL, r.start, PAGE_SIZE, r.size);
		local_flush_tlb_pages(NULL, r.start, HPAGE_SIZE, r.size);
	}
}


extern void flush_tlb_all(void);
extern void flush_tlb_kernel_range(unsigned long start, unsigned long end);
extern void flush_tlb_current_task(void);
extern void flush_tlb_mm(struct mm_struct *);
extern void flush_tlb_page(const struct vm_area_struct *, unsigned long);
extern void flush_tlb_page_mm(const struct vm_area_struct *,
			      struct mm_struct *, unsigned long);
extern void flush_tlb_range(const struct vm_area_struct *,
			    unsigned long start, unsigned long end);

#define flush_tlb()     flush_tlb_current_task()

#endif 
