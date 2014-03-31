/* MN10300 Page table management
 *
 * Copyright (C) 2007 Matsushita Electric Industrial Co., Ltd.
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Modified by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/smp.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/spinlock.h>
#include <linux/quicklist.h>

#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>

void set_pmd_pfn(unsigned long vaddr, unsigned long pfn, pgprot_t flags)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;

	if (vaddr & (PMD_SIZE-1)) {		
		printk(KERN_ERR "set_pmd_pfn: vaddr misaligned\n");
		return; 
	}
	if (pfn & (PTRS_PER_PTE-1)) {		
		printk(KERN_ERR "set_pmd_pfn: pfn misaligned\n");
		return; 
	}
	pgd = swapper_pg_dir + pgd_index(vaddr);
	if (pgd_none(*pgd)) {
		printk(KERN_ERR "set_pmd_pfn: pgd_none\n");
		return; 
	}
	pud = pud_offset(pgd, vaddr);
	pmd = pmd_offset(pud, vaddr);
	set_pmd(pmd, pfn_pmd(pfn, flags));
	local_flush_tlb_one(vaddr);
}

pte_t *pte_alloc_one_kernel(struct mm_struct *mm, unsigned long address)
{
	pte_t *pte = (pte_t *)__get_free_page(GFP_KERNEL|__GFP_REPEAT);
	if (pte)
		clear_page(pte);
	return pte;
}

struct page *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	struct page *pte;

#ifdef CONFIG_HIGHPTE
	pte = alloc_pages(GFP_KERNEL|__GFP_HIGHMEM|__GFP_REPEAT, 0);
#else
	pte = alloc_pages(GFP_KERNEL|__GFP_REPEAT, 0);
#endif
	if (pte)
		clear_highpage(pte);
	return pte;
}

DEFINE_SPINLOCK(pgd_lock);
struct page *pgd_list;

static inline void pgd_list_add(pgd_t *pgd)
{
	struct page *page = virt_to_page(pgd);
	page->index = (unsigned long) pgd_list;
	if (pgd_list)
		set_page_private(pgd_list, (unsigned long) &page->index);
	pgd_list = page;
	set_page_private(page, (unsigned long) &pgd_list);
}

static inline void pgd_list_del(pgd_t *pgd)
{
	struct page *next, **pprev, *page = virt_to_page(pgd);
	next = (struct page *) page->index;
	pprev = (struct page **) page_private(page);
	*pprev = next;
	if (next)
		set_page_private(next, (unsigned long) pprev);
}

void pgd_ctor(void *pgd)
{
	unsigned long flags;

	if (PTRS_PER_PMD == 1)
		spin_lock_irqsave(&pgd_lock, flags);

	memcpy((pgd_t *)pgd + USER_PTRS_PER_PGD,
			swapper_pg_dir + USER_PTRS_PER_PGD,
			(PTRS_PER_PGD - USER_PTRS_PER_PGD) * sizeof(pgd_t));

	if (PTRS_PER_PMD > 1)
		return;

	pgd_list_add(pgd);
	spin_unlock_irqrestore(&pgd_lock, flags);
	memset(pgd, 0, USER_PTRS_PER_PGD * sizeof(pgd_t));
}

void pgd_dtor(void *pgd)
{
	unsigned long flags; 

	spin_lock_irqsave(&pgd_lock, flags);
	pgd_list_del(pgd);
	spin_unlock_irqrestore(&pgd_lock, flags);
}

pgd_t *pgd_alloc(struct mm_struct *mm)
{
	return quicklist_alloc(0, GFP_KERNEL, pgd_ctor);
}

void pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
	quicklist_free(0, pgd_dtor, pgd);
}

void __init pgtable_cache_init(void)
{
}

void check_pgt_cache(void)
{
	quicklist_trim(0, pgd_dtor, 25, 16);
}
