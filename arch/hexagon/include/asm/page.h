/*
 * Page management definitions for the Hexagon architecture
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

#ifndef _ASM_PAGE_H
#define _ASM_PAGE_H

#include <linux/const.h>


#ifdef CONFIG_PAGE_SIZE_4KB
#define PAGE_SHIFT 12
#define HEXAGON_L1_PTE_SIZE __HVM_PDE_S_4KB
#endif

#ifdef CONFIG_PAGE_SIZE_16KB
#define PAGE_SHIFT 14
#define HEXAGON_L1_PTE_SIZE __HVM_PDE_S_16KB
#endif

#ifdef CONFIG_PAGE_SIZE_64KB
#define PAGE_SHIFT 16
#define HEXAGON_L1_PTE_SIZE __HVM_PDE_S_64KB
#endif

#ifdef CONFIG_PAGE_SIZE_256KB
#define PAGE_SHIFT 18
#define HEXAGON_L1_PTE_SIZE __HVM_PDE_S_256KB
#endif

#ifdef CONFIG_PAGE_SIZE_1MB
#define PAGE_SHIFT 20
#define HEXAGON_L1_PTE_SIZE __HVM_PDE_S_1MB
#endif

#ifdef CONFIG_HUGETLB_PAGE
#define HPAGE_SHIFT 22
#define HPAGE_SIZE (1UL << HPAGE_SHIFT)
#define HPAGE_MASK (~(HPAGE_SIZE-1))
#define HUGETLB_PAGE_ORDER (HPAGE_SHIFT-PAGE_SHIFT)
#define HVM_HUGEPAGE_SIZE 0x5
#endif

#define PAGE_SIZE  (1UL << PAGE_SHIFT)
#define PAGE_MASK  (~((1 << PAGE_SHIFT) - 1))

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#include <linux/pfn.h>

typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long pgd; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;
typedef struct page *pgtable_t;

#define pte_val(x)     ((x).pte)
#define pgd_val(x)     ((x).pgd)
#define pgprot_val(x)  ((x).pgprot)
#define __pte(x)       ((pte_t) { (x) })
#define __pgd(x)       ((pgd_t) { (x) })
#define __pgprot(x)    ((pgprot_t) { (x) })

#define __pa(x) ((unsigned long)(x) - PAGE_OFFSET)
#define __va(x) ((void *)((unsigned long)(x) + PAGE_OFFSET))

struct page;

#define virt_to_page(kaddr) pfn_to_page(PFN_DOWN(__pa(kaddr)))

#define VM_DATA_DEFAULT_FLAGS (VM_READ | VM_WRITE | \
				VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#define pfn_valid(pfn) ((pfn) < max_mapnr)
#define virt_addr_valid(kaddr) pfn_valid(__pa(kaddr) >> PAGE_SHIFT)

static inline void clear_page(void *page)
{
	
	asm volatile(
		"	loop0(1f,%1);\n"
		"1:	{ dczeroa(%0);\n"
		"	  %0 = add(%0,#32); }:endloop0\n"
		: "+r" (page)
		: "r" (PAGE_SIZE/32)
		: "lc0", "sa0", "memory"
	);
}

#define copy_page(to, from)	memcpy((to), (from), PAGE_SIZE)

#define clear_user_page(page, vaddr, pg)	clear_page(page)
#define copy_user_page(to, from, vaddr, pg)	copy_page(to, from)

#define page_to_phys(page)      (page_to_pfn(page) << PAGE_SHIFT)

#define kern_addr_valid(addr)   (1)

#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>

#endif 
#endif 

#endif
