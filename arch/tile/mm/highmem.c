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

#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <asm/homecache.h>

#define kmap_get_pte(vaddr) \
	pte_offset_kernel(pmd_offset(pud_offset(pgd_offset_k(vaddr), (vaddr)),\
		(vaddr)), (vaddr))


void *kmap(struct page *page)
{
	void *kva;
	unsigned long flags;
	pte_t *ptep;

	might_sleep();
	if (!PageHighMem(page))
		return page_address(page);
	kva = kmap_high(page);

	ptep = kmap_get_pte((unsigned long)kva);
	flags = homecache_kpte_lock();
	set_pte_at(&init_mm, kva, ptep, mk_pte(page, page_to_kpgprot(page)));
	homecache_kpte_unlock(flags);

	return kva;
}
EXPORT_SYMBOL(kmap);

void kunmap(struct page *page)
{
	if (in_interrupt())
		BUG();
	if (!PageHighMem(page))
		return;
	kunmap_high(page);
}
EXPORT_SYMBOL(kunmap);

struct atomic_mapped_page {
	struct list_head list;
	struct page *page;
	int cpu;
	unsigned long va;
};

static spinlock_t amp_lock = __SPIN_LOCK_UNLOCKED(&amp_lock);
static struct list_head amp_list = LIST_HEAD_INIT(amp_list);

struct kmap_amps {
	struct atomic_mapped_page per_type[KM_TYPE_NR];
};
static DEFINE_PER_CPU(struct kmap_amps, amps);

static void kmap_atomic_register(struct page *page, enum km_type type,
				 unsigned long va, pte_t *ptep, pte_t pteval)
{
	unsigned long flags;
	struct atomic_mapped_page *amp;

	flags = homecache_kpte_lock();
	spin_lock(&amp_lock);

	
	amp = &__get_cpu_var(amps).per_type[type];
	amp->page = page;
	amp->cpu = smp_processor_id();
	amp->va = va;

	
	if (!pte_read(pteval))
		pteval = mk_pte(page, page_to_kpgprot(page));

	list_add(&amp->list, &amp_list);
	set_pte(ptep, pteval);
	arch_flush_lazy_mmu_mode();

	spin_unlock(&amp_lock);
	homecache_kpte_unlock(flags);
}

static void kmap_atomic_unregister(struct page *page, unsigned long va)
{
	unsigned long flags;
	struct atomic_mapped_page *amp;
	int cpu = smp_processor_id();
	spin_lock_irqsave(&amp_lock, flags);
	list_for_each_entry(amp, &amp_list, list) {
		if (amp->page == page && amp->cpu == cpu && amp->va == va)
			break;
	}
	BUG_ON(&amp->list == &amp_list);
	list_del(&amp->list);
	spin_unlock_irqrestore(&amp_lock, flags);
}

static void kmap_atomic_fix_one_kpte(struct atomic_mapped_page *amp,
				     int finished)
{
	pte_t *ptep = kmap_get_pte(amp->va);
	if (!finished) {
		set_pte(ptep, pte_mkmigrate(*ptep));
		flush_remote(0, 0, NULL, amp->va, PAGE_SIZE, PAGE_SIZE,
			     cpumask_of(amp->cpu), NULL, 0);
	} else {
		pte_t pte = mk_pte(amp->page, page_to_kpgprot(amp->page));
		set_pte(ptep, pte);
	}
}

void kmap_atomic_fix_kpte(struct page *page, int finished)
{
	struct atomic_mapped_page *amp;
	unsigned long flags;
	spin_lock_irqsave(&amp_lock, flags);
	list_for_each_entry(amp, &amp_list, list) {
		if (amp->page == page)
			kmap_atomic_fix_one_kpte(amp, finished);
	}
	spin_unlock_irqrestore(&amp_lock, flags);
}

void *kmap_atomic_prot(struct page *page, pgprot_t prot)
{
	unsigned long vaddr;
	int idx, type;
	pte_t *pte;

	
	pagefault_disable();

	
	BUG_ON(pte_exec(prot));

	if (!PageHighMem(page))
		return page_address(page);

	type = kmap_atomic_idx_push();
	idx = type + KM_TYPE_NR*smp_processor_id();
	vaddr = __fix_to_virt(FIX_KMAP_BEGIN + idx);
	pte = kmap_get_pte(vaddr);
	BUG_ON(!pte_none(*pte));

	
	kmap_atomic_register(page, type, vaddr, pte, mk_pte(page, prot));

	return (void *)vaddr;
}
EXPORT_SYMBOL(kmap_atomic_prot);

void *kmap_atomic(struct page *page)
{
	
	return kmap_atomic_prot(page, PAGE_NONE);
}
EXPORT_SYMBOL(kmap_atomic);

void __kunmap_atomic(void *kvaddr)
{
	unsigned long vaddr = (unsigned long) kvaddr & PAGE_MASK;

	if (vaddr >= __fix_to_virt(FIX_KMAP_END) &&
	    vaddr <= __fix_to_virt(FIX_KMAP_BEGIN)) {
		pte_t *pte = kmap_get_pte(vaddr);
		pte_t pteval = *pte;
		int idx, type;

		type = kmap_atomic_idx();
		idx = type + KM_TYPE_NR*smp_processor_id();

		BUG_ON(!pte_present(pteval) && !pte_migrating(pteval));
		kmap_atomic_unregister(pte_page(pteval), vaddr);
		kpte_clear_flush(pte, vaddr);
		kmap_atomic_idx_pop();
	} else {
		
		BUG_ON(vaddr < PAGE_OFFSET);
		BUG_ON(vaddr >= (unsigned long)high_memory);
	}

	arch_flush_lazy_mmu_mode();
	pagefault_enable();
}
EXPORT_SYMBOL(__kunmap_atomic);

void *kmap_atomic_pfn(unsigned long pfn)
{
	return kmap_atomic(pfn_to_page(pfn));
}
void *kmap_atomic_prot_pfn(unsigned long pfn, pgprot_t prot)
{
	return kmap_atomic_prot(pfn_to_page(pfn), prot);
}

struct page *kmap_atomic_to_page(void *ptr)
{
	pte_t *pte;
	unsigned long vaddr = (unsigned long)ptr;

	if (vaddr < FIXADDR_START)
		return virt_to_page(ptr);

	pte = kmap_get_pte(vaddr);
	return pte_page(*pte);
}
