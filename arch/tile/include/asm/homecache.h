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
 *
 * Handle issues around the Tile "home cache" model of coherence.
 */

#ifndef _ASM_TILE_HOMECACHE_H
#define _ASM_TILE_HOMECACHE_H

#include <asm/page.h>
#include <linux/cpumask.h>

struct page;
struct task_struct;
struct vm_area_struct;
struct zone;

#define PAGE_HOME_UNCACHED -1

#define PAGE_HOME_IMMUTABLE -2

#define PAGE_HOME_INCOHERENT -3

#if CHIP_HAS_CBOX_HOME_MAP()
#define PAGE_HOME_HASH -4
#endif

#define PAGE_HOME_UNKNOWN -5

#define PAGE_HOME_HERE -6

extern void flush_remote(unsigned long cache_pfn, unsigned long cache_length,
			 const struct cpumask *cache_cpumask,
			 HV_VirtAddr tlb_va, unsigned long tlb_length,
			 unsigned long tlb_pgsize,
			 const struct cpumask *tlb_cpumask,
			 HV_Remote_ASID *asids, int asidcount);

extern pte_t pte_set_home(pte_t pte, int home);

extern void homecache_evict(const struct cpumask *mask);

extern void homecache_change_page_home(struct page *, int order, int home);

extern void homecache_flush_cache(struct page *, int order);

extern struct page *homecache_alloc_pages(gfp_t gfp_mask,
					  unsigned int order, int home);
extern struct page *homecache_alloc_pages_node(int nid, gfp_t gfp_mask,
					       unsigned int order, int home);
#define homecache_alloc_page(gfp_mask, home) \
  homecache_alloc_pages(gfp_mask, 0, home)

void homecache_free_pages(unsigned long addr, unsigned int order);
#define homecache_free_page(page) \
  homecache_free_pages((page), 0)



extern int page_home(struct page *);

#define homecache_migrate_kthread() do {} while (0)

#define homecache_kpte_lock() 0
#define homecache_kpte_unlock(flags) do {} while (0)


#endif 
