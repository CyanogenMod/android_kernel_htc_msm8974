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

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/spinlock.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/smp.h>

#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/fixmap.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>
#include <asm/homecache.h>

#define K(x) ((x) << (PAGE_SHIFT-10))

void show_mem(unsigned int filter)
{
	struct zone *zone;

	pr_err("Active:%lu inactive:%lu dirty:%lu writeback:%lu unstable:%lu"
	       " free:%lu\n slab:%lu mapped:%lu pagetables:%lu bounce:%lu"
	       " pagecache:%lu swap:%lu\n",
	       (global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE)),
	       (global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE)),
	       global_page_state(NR_FILE_DIRTY),
	       global_page_state(NR_WRITEBACK),
	       global_page_state(NR_UNSTABLE_NFS),
	       global_page_state(NR_FREE_PAGES),
	       (global_page_state(NR_SLAB_RECLAIMABLE) +
		global_page_state(NR_SLAB_UNRECLAIMABLE)),
	       global_page_state(NR_FILE_MAPPED),
	       global_page_state(NR_PAGETABLE),
	       global_page_state(NR_BOUNCE),
	       global_page_state(NR_FILE_PAGES),
	       get_nr_swap_pages());

	for_each_zone(zone) {
		unsigned long flags, order, total = 0, largest_order = -1;

		if (!populated_zone(zone))
			continue;

		spin_lock_irqsave(&zone->lock, flags);
		for (order = 0; order < MAX_ORDER; order++) {
			int nr = zone->free_area[order].nr_free;
			total += nr << order;
			if (nr)
				largest_order = order;
		}
		spin_unlock_irqrestore(&zone->lock, flags);
		pr_err("Node %d %7s: %lukB (largest %luKb)\n",
		       zone_to_nid(zone), zone->name,
		       K(total), largest_order ? K(1UL) << largest_order : 0);
	}
}

static void set_pte_pfn(unsigned long vaddr, unsigned long pfn, pgprot_t flags)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = swapper_pg_dir + pgd_index(vaddr);
	if (pgd_none(*pgd)) {
		BUG();
		return;
	}
	pud = pud_offset(pgd, vaddr);
	if (pud_none(*pud)) {
		BUG();
		return;
	}
	pmd = pmd_offset(pud, vaddr);
	if (pmd_none(*pmd)) {
		BUG();
		return;
	}
	pte = pte_offset_kernel(pmd, vaddr);
	
	set_pte(pte, pfn_pte(pfn, flags));

	local_flush_tlb_page(NULL, vaddr, PAGE_SIZE);
}

void __set_fixmap(enum fixed_addresses idx, unsigned long phys, pgprot_t flags)
{
	unsigned long address = __fix_to_virt(idx);

	if (idx >= __end_of_fixed_addresses) {
		BUG();
		return;
	}
	set_pte_pfn(address, phys >> PAGE_SHIFT, flags);
}

#if defined(CONFIG_HIGHPTE)
pte_t *_pte_offset_map(pmd_t *dir, unsigned long address)
{
	pte_t *pte = kmap_atomic(pmd_page(*dir)) +
		(pmd_ptfn(*dir) << HV_LOG2_PAGE_TABLE_ALIGN) & ~PAGE_MASK;
	return &pte[pte_index(address)];
}
#endif

void shatter_huge_page(unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	unsigned long flags = 0;  
#ifdef __PAGETABLE_PMD_FOLDED
	struct list_head *pos;
#endif

	
	addr &= HPAGE_MASK;
	BUG_ON(pgd_addr_invalid(addr));
	BUG_ON(addr < PAGE_OFFSET);  
	pgd = swapper_pg_dir + pgd_index(addr);
	pud = pud_offset(pgd, addr);
	BUG_ON(!pud_present(*pud));
	pmd = pmd_offset(pud, addr);
	BUG_ON(!pmd_present(*pmd));
	if (!pmd_huge_page(*pmd))
		return;

	spin_lock_irqsave(&init_mm.page_table_lock, flags);
	if (!pmd_huge_page(*pmd)) {
		
		spin_unlock_irqrestore(&init_mm.page_table_lock, flags);
		return;
	}

	
	pmd_populate_kernel(&init_mm, pmd,
			    get_prealloc_pte(pte_pfn(*(pte_t *)pmd)));

#ifdef __PAGETABLE_PMD_FOLDED
	
	spin_lock(&pgd_lock);
	list_for_each(pos, &pgd_list) {
		pmd_t *copy_pmd;
		pgd = list_to_pgd(pos) + pgd_index(addr);
		pud = pud_offset(pgd, addr);
		copy_pmd = pmd_offset(pud, addr);
		__set_pmd(copy_pmd, *pmd);
	}
	spin_unlock(&pgd_lock);
#endif

	
	flush_remote(0, 0, NULL, addr, HPAGE_SIZE, HPAGE_SIZE,
		     cpu_possible_mask, NULL, 0);

	
	spin_unlock_irqrestore(&init_mm.page_table_lock, flags);
}

DEFINE_SPINLOCK(pgd_lock);
LIST_HEAD(pgd_list);

static inline void pgd_list_add(pgd_t *pgd)
{
	list_add(pgd_to_list(pgd), &pgd_list);
}

static inline void pgd_list_del(pgd_t *pgd)
{
	list_del(pgd_to_list(pgd));
}

#define KERNEL_PGD_INDEX_START pgd_index(PAGE_OFFSET)
#define KERNEL_PGD_PTRS (PTRS_PER_PGD - KERNEL_PGD_INDEX_START)

static void pgd_ctor(pgd_t *pgd)
{
	unsigned long flags;

	memset(pgd, 0, KERNEL_PGD_INDEX_START*sizeof(pgd_t));
	spin_lock_irqsave(&pgd_lock, flags);

#ifndef __tilegx__
	BUG_ON(((u64 *)swapper_pg_dir)[pgd_index(MEM_USER_INTRPT)] != 0);
#endif

	memcpy(pgd + KERNEL_PGD_INDEX_START,
	       swapper_pg_dir + KERNEL_PGD_INDEX_START,
	       KERNEL_PGD_PTRS * sizeof(pgd_t));

	pgd_list_add(pgd);
	spin_unlock_irqrestore(&pgd_lock, flags);
}

static void pgd_dtor(pgd_t *pgd)
{
	unsigned long flags; 

	spin_lock_irqsave(&pgd_lock, flags);
	pgd_list_del(pgd);
	spin_unlock_irqrestore(&pgd_lock, flags);
}

pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd = kmem_cache_alloc(pgd_cache, GFP_KERNEL);
	if (pgd)
		pgd_ctor(pgd);
	return pgd;
}

void pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
	pgd_dtor(pgd);
	kmem_cache_free(pgd_cache, pgd);
}


#define L2_USER_PGTABLE_PAGES (1 << L2_USER_PGTABLE_ORDER)

struct page *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	gfp_t flags = GFP_KERNEL|__GFP_REPEAT|__GFP_ZERO;
	struct page *p;
#if L2_USER_PGTABLE_ORDER > 0
	int i;
#endif

#ifdef CONFIG_HIGHPTE
	flags |= __GFP_HIGHMEM;
#endif

	p = alloc_pages(flags, L2_USER_PGTABLE_ORDER);
	if (p == NULL)
		return NULL;

#if L2_USER_PGTABLE_ORDER > 0
	for (i = 1; i < L2_USER_PGTABLE_PAGES; ++i) {
		init_page_count(p+i);
		inc_zone_page_state(p+i, NR_PAGETABLE);
	}
#endif

	pgtable_page_ctor(p);
	return p;
}

void pte_free(struct mm_struct *mm, struct page *p)
{
	int i;

	pgtable_page_dtor(p);
	__free_page(p);

	for (i = 1; i < L2_USER_PGTABLE_PAGES; ++i) {
		__free_page(p+i);
		dec_zone_page_state(p+i, NR_PAGETABLE);
	}
}

void __pte_free_tlb(struct mmu_gather *tlb, struct page *pte,
		    unsigned long address)
{
	int i;

	pgtable_page_dtor(pte);
	tlb_remove_page(tlb, pte);

	for (i = 1; i < L2_USER_PGTABLE_PAGES; ++i) {
		tlb_remove_page(tlb, pte + i);
		dec_zone_page_state(pte + i, NR_PAGETABLE);
	}
}

#ifndef __tilegx__

int ptep_test_and_clear_young(struct vm_area_struct *vma,
			      unsigned long addr, pte_t *ptep)
{
#if HV_PTE_INDEX_ACCESSED < 8 || HV_PTE_INDEX_ACCESSED >= 16
# error Code assumes HV_PTE "accessed" bit in second byte
#endif
	u8 *tmp = (u8 *)ptep;
	u8 second_byte = tmp[1];
	if (!(second_byte & (1 << (HV_PTE_INDEX_ACCESSED - 8))))
		return 0;
	tmp[1] = second_byte & ~(1 << (HV_PTE_INDEX_ACCESSED - 8));
	return 1;
}

void ptep_set_wrprotect(struct mm_struct *mm,
			unsigned long addr, pte_t *ptep)
{
#if HV_PTE_INDEX_WRITABLE < 32
# error Code assumes HV_PTE "writable" bit in high word
#endif
	u32 *tmp = (u32 *)ptep;
	tmp[1] = tmp[1] & ~(1 << (HV_PTE_INDEX_WRITABLE - 32));
}

#endif

pte_t *virt_to_pte(struct mm_struct* mm, unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;

	if (pgd_addr_invalid(addr))
		return NULL;

	pgd = mm ? pgd_offset(mm, addr) : swapper_pg_dir + pgd_index(addr);
	pud = pud_offset(pgd, addr);
	if (!pud_present(*pud))
		return NULL;
	pmd = pmd_offset(pud, addr);
	if (pmd_huge_page(*pmd))
		return (pte_t *)pmd;
	if (!pmd_present(*pmd))
		return NULL;
	return pte_offset_kernel(pmd, addr);
}

pgprot_t set_remote_cache_cpu(pgprot_t prot, int cpu)
{
	unsigned int width = smp_width;
	int x = cpu % width;
	int y = cpu / width;
	BUG_ON(y >= smp_height);
	BUG_ON(hv_pte_get_mode(prot) != HV_PTE_MODE_CACHE_TILE_L3);
	BUG_ON(cpu < 0 || cpu >= NR_CPUS);
	BUG_ON(!cpu_is_valid_lotar(cpu));
	return hv_pte_set_lotar(prot, HV_XY_TO_LOTAR(x, y));
}

int get_remote_cache_cpu(pgprot_t prot)
{
	HV_LOTAR lotar = hv_pte_get_lotar(prot);
	int x = HV_LOTAR_X(lotar);
	int y = HV_LOTAR_Y(lotar);
	BUG_ON(hv_pte_get_mode(prot) != HV_PTE_MODE_CACHE_TILE_L3);
	return x + y * smp_width;
}

int va_to_cpa_and_pte(void *va, unsigned long long *cpa, pte_t *pte)
{
	struct page *page = virt_to_page(va);
	pte_t null_pte = { 0 };

	*cpa = __pa(va);

	
	*pte = pte_set_home(null_pte, page_home(page));

	return 0; 
}
EXPORT_SYMBOL(va_to_cpa_and_pte);

void __set_pte(pte_t *ptep, pte_t pte)
{
#ifdef __tilegx__
	*ptep = pte;
#else
# if HV_PTE_INDEX_PRESENT >= 32 || HV_PTE_INDEX_MIGRATING >= 32
#  error Must write the present and migrating bits last
# endif
	if (pte_present(pte)) {
		((u32 *)ptep)[1] = (u32)(pte_val(pte) >> 32);
		barrier();
		((u32 *)ptep)[0] = (u32)(pte_val(pte));
	} else {
		((u32 *)ptep)[0] = (u32)(pte_val(pte));
		barrier();
		((u32 *)ptep)[1] = (u32)(pte_val(pte) >> 32);
	}
#endif 
}

void set_pte(pte_t *ptep, pte_t pte)
{
	if (pte_present(pte) &&
	    (!CHIP_HAS_MMIO() || hv_pte_get_mode(pte) != HV_PTE_MODE_MMIO)) {
		
		unsigned long pfn = pte_pfn(pte);
		if (pfn_valid(pfn)) {
			
			pte = pte_set_home(pte, page_home(pfn_to_page(pfn)));
		} else if (hv_pte_get_mode(pte) == 0) {
			
			panic("set_pte(): out-of-range PFN and mode 0\n");
		}
	}

	__set_pte(ptep, pte);
}

static inline int mm_is_priority_cached(struct mm_struct *mm)
{
	return mm->context.priority_cached;
}

void start_mm_caching(struct mm_struct *mm)
{
	if (!mm_is_priority_cached(mm)) {
		mm->context.priority_cached = -1U;
		hv_set_caching(-1U);
	}
}

static unsigned int update_priority_cached(struct mm_struct *mm)
{
	if (mm->context.priority_cached && down_write_trylock(&mm->mmap_sem)) {
		struct vm_area_struct *vm;
		for (vm = mm->mmap; vm; vm = vm->vm_next) {
			if (hv_pte_get_cached_priority(vm->vm_page_prot))
				break;
		}
		if (vm == NULL)
			mm->context.priority_cached = 0;
		up_write(&mm->mmap_sem);
	}
	return mm->context.priority_cached;
}

void check_mm_caching(struct mm_struct *prev, struct mm_struct *next)
{
	if (!mm_is_priority_cached(next)) {
		if (mm_is_priority_cached(prev))
			hv_set_caching(0);
	} else {
		hv_set_caching(update_priority_cached(next));
	}
}

#if CHIP_HAS_MMIO()

void __iomem *ioremap_prot(resource_size_t phys_addr, unsigned long size,
			   pgprot_t home)
{
	void *addr;
	struct vm_struct *area;
	unsigned long offset, last_addr;
	pgprot_t pgprot;

	
	last_addr = phys_addr + size - 1;
	if (!size || last_addr < phys_addr)
		return NULL;

	
	pgprot = PAGE_KERNEL;
	pgprot = hv_pte_set_mode(pgprot, HV_PTE_MODE_MMIO);
	pgprot = hv_pte_set_lotar(pgprot, hv_pte_get_lotar(home));

	offset = phys_addr & ~PAGE_MASK;
	phys_addr &= PAGE_MASK;
	size = PAGE_ALIGN(last_addr+1) - phys_addr;

	area = get_vm_area(size, VM_IOREMAP );
	if (!area)
		return NULL;
	area->phys_addr = phys_addr;
	addr = area->addr;
	if (ioremap_page_range((unsigned long)addr, (unsigned long)addr + size,
			       phys_addr, pgprot)) {
		remove_vm_area((void *)(PAGE_MASK & (unsigned long) addr));
		return NULL;
	}
	return (__force void __iomem *) (offset + (char *)addr);
}
EXPORT_SYMBOL(ioremap_prot);

void __iomem *ioremap(resource_size_t phys_addr, unsigned long size)
{
	panic("ioremap for PCI MMIO is not supported");
}
EXPORT_SYMBOL(ioremap);

void iounmap(volatile void __iomem *addr_in)
{
	volatile void __iomem *addr = (volatile void __iomem *)
		(PAGE_MASK & (unsigned long __force)addr_in);
#if 1
	vunmap((void * __force)addr);
#else
	struct vm_struct *p, *o;

	read_lock(&vmlist_lock);
	for (p = vmlist; p; p = p->next) {
		if (p->addr == addr)
			break;
	}
	read_unlock(&vmlist_lock);

	if (!p) {
		pr_err("iounmap: bad address %p\n", addr);
		dump_stack();
		return;
	}

	
	o = remove_vm_area((void *)addr);
	BUG_ON(p != o || o == NULL);
	kfree(p);
#endif
}
EXPORT_SYMBOL(iounmap);

#endif 
