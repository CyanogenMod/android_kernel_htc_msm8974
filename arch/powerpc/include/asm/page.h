#ifndef _ASM_POWERPC_PAGE_H
#define _ASM_POWERPC_PAGE_H

/*
 * Copyright (C) 2001,2005 IBM Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef __ASSEMBLY__
#include <linux/types.h>
#else
#include <asm/types.h>
#endif
#include <asm/asm-compat.h>
#include <asm/kdump.h>

#if defined(CONFIG_PPC_256K_PAGES)
#define PAGE_SHIFT		18
#elif defined(CONFIG_PPC_64K_PAGES)
#define PAGE_SHIFT		16
#elif defined(CONFIG_PPC_16K_PAGES)
#define PAGE_SHIFT		14
#else
#define PAGE_SHIFT		12
#endif

#define PAGE_SIZE		(ASM_CONST(1) << PAGE_SHIFT)

#ifndef __ASSEMBLY__
#ifdef CONFIG_HUGETLB_PAGE
extern unsigned int HPAGE_SHIFT;
#else
#define HPAGE_SHIFT PAGE_SHIFT
#endif
#define HPAGE_SIZE		((1UL) << HPAGE_SHIFT)
#define HPAGE_MASK		(~(HPAGE_SIZE - 1))
#define HUGETLB_PAGE_ORDER	(HPAGE_SHIFT - PAGE_SHIFT)
#define HUGE_MAX_HSTATE		(MMU_PAGE_COUNT-1)
#endif

#define __HAVE_ARCH_GATE_AREA		1

#define PAGE_MASK      (~((1 << PAGE_SHIFT) - 1))


#define KERNELBASE      ASM_CONST(CONFIG_KERNEL_START)
#define PAGE_OFFSET	ASM_CONST(CONFIG_PAGE_OFFSET)
#define LOAD_OFFSET	ASM_CONST((CONFIG_KERNEL_START-CONFIG_PHYSICAL_START))

#if defined(CONFIG_NONSTATIC_KERNEL)
#ifndef __ASSEMBLY__

extern phys_addr_t memstart_addr;
extern phys_addr_t kernstart_addr;

#ifdef CONFIG_RELOCATABLE_PPC32
extern long long virt_phys_offset;
#endif

#endif 
#define PHYSICAL_START	kernstart_addr

#else	
#define PHYSICAL_START	ASM_CONST(CONFIG_PHYSICAL_START)
#endif

#ifdef CONFIG_RELOCATABLE_PPC32
#define VIRT_PHYS_OFFSET virt_phys_offset
#else
#define VIRT_PHYS_OFFSET (KERNELBASE - PHYSICAL_START)
#endif


#ifdef CONFIG_PPC64
#define MEMORY_START	0UL
#elif defined(CONFIG_NONSTATIC_KERNEL)
#define MEMORY_START	memstart_addr
#else
#define MEMORY_START	(PHYSICAL_START + PAGE_OFFSET - KERNELBASE)
#endif

#ifdef CONFIG_FLATMEM
#define ARCH_PFN_OFFSET		((unsigned long)(MEMORY_START >> PAGE_SHIFT))
#define pfn_valid(pfn)		((pfn) >= ARCH_PFN_OFFSET && (pfn) < max_mapnr)
#endif

#define virt_to_page(kaddr)	pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)
#define pfn_to_kaddr(pfn)	__va((pfn) << PAGE_SHIFT)
#define virt_addr_valid(kaddr)	pfn_valid(__pa(kaddr) >> PAGE_SHIFT)

#ifdef CONFIG_BOOKE
#define __va(x) ((void *)(unsigned long)((phys_addr_t)(x) + VIRT_PHYS_OFFSET))
#define __pa(x) ((unsigned long)(x) - VIRT_PHYS_OFFSET)
#else
#define __va(x) ((void *)(unsigned long)((phys_addr_t)(x) + PAGE_OFFSET - MEMORY_START))
#define __pa(x) ((unsigned long)(x) - PAGE_OFFSET + MEMORY_START)
#endif

#define VM_DATA_DEFAULT_FLAGS32	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#define VM_DATA_DEFAULT_FLAGS64	(VM_READ | VM_WRITE | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#ifdef __powerpc64__
#include <asm/page_64.h>
#else
#include <asm/page_32.h>
#endif

#define _ALIGN_UP(addr,size)	(((addr)+((size)-1))&(~((size)-1)))
#define _ALIGN_DOWN(addr,size)	((addr)&(~((size)-1)))

#define _ALIGN(addr,size)     _ALIGN_UP(addr,size)

#ifdef CONFIG_PPC_BOOK3E_64
#define is_kernel_addr(x)	((x) >= 0x8000000000000000ul)
#else
#define is_kernel_addr(x)	((x) >= PAGE_OFFSET)
#endif

#ifdef CONFIG_PPC64
#define PD_HUGE 0x8000000000000000
#else
#define PD_HUGE 0x80000000
#endif

#define HUGEPD_SHIFT_MASK     0x3f

#ifndef __ASSEMBLY__

#undef STRICT_MM_TYPECHECKS

#ifdef STRICT_MM_TYPECHECKS

typedef struct { pte_basic_t pte; } pte_t;
#define pte_val(x)	((x).pte)
#define __pte(x)	((pte_t) { (x) })

#if defined(CONFIG_PPC_64K_PAGES) && defined(CONFIG_PPC_STD_MMU_64)
typedef struct { pte_t pte; unsigned long hidx; } real_pte_t;
#else
typedef struct { pte_t pte; } real_pte_t;
#endif

#ifdef CONFIG_PPC64
typedef struct { unsigned long pmd; } pmd_t;
#define pmd_val(x)	((x).pmd)
#define __pmd(x)	((pmd_t) { (x) })

#ifndef CONFIG_PPC_64K_PAGES
typedef struct { unsigned long pud; } pud_t;
#define pud_val(x)	((x).pud)
#define __pud(x)	((pud_t) { (x) })
#endif 
#endif 

typedef struct { unsigned long pgd; } pgd_t;
#define pgd_val(x)	((x).pgd)
#define __pgd(x)	((pgd_t) { (x) })

typedef struct { unsigned long pgprot; } pgprot_t;
#define pgprot_val(x)	((x).pgprot)
#define __pgprot(x)	((pgprot_t) { (x) })

#else


typedef pte_basic_t pte_t;
#define pte_val(x)	(x)
#define __pte(x)	(x)

#if defined(CONFIG_PPC_64K_PAGES) && defined(CONFIG_PPC_STD_MMU_64)
typedef struct { pte_t pte; unsigned long hidx; } real_pte_t;
#else
typedef pte_t real_pte_t;
#endif


#ifdef CONFIG_PPC64
typedef unsigned long pmd_t;
#define pmd_val(x)	(x)
#define __pmd(x)	(x)

#ifndef CONFIG_PPC_64K_PAGES
typedef unsigned long pud_t;
#define pud_val(x)	(x)
#define __pud(x)	(x)
#endif 
#endif 

typedef unsigned long pgd_t;
#define pgd_val(x)	(x)
#define pgprot_val(x)	(x)

typedef unsigned long pgprot_t;
#define __pgd(x)	(x)
#define __pgprot(x)	(x)

#endif

typedef struct { signed long pd; } hugepd_t;

#ifdef CONFIG_HUGETLB_PAGE
static inline int hugepd_ok(hugepd_t hpd)
{
	return (hpd.pd > 0);
}

#define is_hugepd(pdep)               (hugepd_ok(*((hugepd_t *)(pdep))))
#else 
#define is_hugepd(pdep)			0
#endif 

struct page;
extern void clear_user_page(void *page, unsigned long vaddr, struct page *pg);
extern void copy_user_page(void *to, void *from, unsigned long vaddr,
		struct page *p);
extern int page_is_ram(unsigned long pfn);
extern int devmem_is_allowed(unsigned long pfn);

#ifdef CONFIG_PPC_SMLPAR
void arch_free_page(struct page *page, int order);
#define HAVE_ARCH_FREE_PAGE
#endif

struct vm_area_struct;

typedef struct page *pgtable_t;

#include <asm-generic/memory_model.h>
#endif 

#endif 
