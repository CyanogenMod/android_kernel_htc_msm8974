/*
 * Lockless get_user_pages_fast for x86
 *
 * Copyright (C) 2008 Nick Piggin
 * Copyright (C) 2008 Novell Inc.
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/vmstat.h>
#include <linux/highmem.h>
#include <linux/swap.h>

#include <asm/pgtable.h>

static inline pte_t gup_get_pte(pte_t *ptep)
{
#ifndef CONFIG_X86_PAE
	return ACCESS_ONCE(*ptep);
#else
	pte_t pte;

retry:
	pte.pte_low = ptep->pte_low;
	smp_rmb();
	pte.pte_high = ptep->pte_high;
	smp_rmb();
	if (unlikely(pte.pte_low != ptep->pte_low))
		goto retry;

	return pte;
#endif
}

static noinline int gup_pte_range(pmd_t pmd, unsigned long addr,
		unsigned long end, int write, struct page **pages, int *nr)
{
	unsigned long mask;
	pte_t *ptep;

	mask = _PAGE_PRESENT|_PAGE_USER;
	if (write)
		mask |= _PAGE_RW;

	ptep = pte_offset_map(&pmd, addr);
	do {
		pte_t pte = gup_get_pte(ptep);
		struct page *page;

		if ((pte_flags(pte) & (mask | _PAGE_SPECIAL)) != mask) {
			pte_unmap(ptep);
			return 0;
		}
		VM_BUG_ON(!pfn_valid(pte_pfn(pte)));
		page = pte_page(pte);
		get_page(page);
		SetPageReferenced(page);
		pages[*nr] = page;
		(*nr)++;

	} while (ptep++, addr += PAGE_SIZE, addr != end);
	pte_unmap(ptep - 1);

	return 1;
}

static inline void get_head_page_multiple(struct page *page, int nr)
{
	VM_BUG_ON(page != compound_head(page));
	VM_BUG_ON(page_count(page) == 0);
	atomic_add(nr, &page->_count);
	SetPageReferenced(page);
}

static noinline int gup_huge_pmd(pmd_t pmd, unsigned long addr,
		unsigned long end, int write, struct page **pages, int *nr)
{
	unsigned long mask;
	pte_t pte = *(pte_t *)&pmd;
	struct page *head, *page;
	int refs;

	mask = _PAGE_PRESENT|_PAGE_USER;
	if (write)
		mask |= _PAGE_RW;
	if ((pte_flags(pte) & mask) != mask)
		return 0;
	
	VM_BUG_ON(pte_flags(pte) & _PAGE_SPECIAL);
	VM_BUG_ON(!pfn_valid(pte_pfn(pte)));

	refs = 0;
	head = pte_page(pte);
	page = head + ((addr & ~PMD_MASK) >> PAGE_SHIFT);
	do {
		VM_BUG_ON(compound_head(page) != head);
		pages[*nr] = page;
		if (PageTail(page))
			get_huge_page_tail(page);
		(*nr)++;
		page++;
		refs++;
	} while (addr += PAGE_SIZE, addr != end);
	get_head_page_multiple(head, refs);

	return 1;
}

static int gup_pmd_range(pud_t pud, unsigned long addr, unsigned long end,
		int write, struct page **pages, int *nr)
{
	unsigned long next;
	pmd_t *pmdp;

	pmdp = pmd_offset(&pud, addr);
	do {
		pmd_t pmd = *pmdp;

		next = pmd_addr_end(addr, end);
		if (pmd_none(pmd) || pmd_trans_splitting(pmd))
			return 0;
		if (unlikely(pmd_large(pmd))) {
			if (!gup_huge_pmd(pmd, addr, next, write, pages, nr))
				return 0;
		} else {
			if (!gup_pte_range(pmd, addr, next, write, pages, nr))
				return 0;
		}
	} while (pmdp++, addr = next, addr != end);

	return 1;
}

static noinline int gup_huge_pud(pud_t pud, unsigned long addr,
		unsigned long end, int write, struct page **pages, int *nr)
{
	unsigned long mask;
	pte_t pte = *(pte_t *)&pud;
	struct page *head, *page;
	int refs;

	mask = _PAGE_PRESENT|_PAGE_USER;
	if (write)
		mask |= _PAGE_RW;
	if ((pte_flags(pte) & mask) != mask)
		return 0;
	
	VM_BUG_ON(pte_flags(pte) & _PAGE_SPECIAL);
	VM_BUG_ON(!pfn_valid(pte_pfn(pte)));

	refs = 0;
	head = pte_page(pte);
	page = head + ((addr & ~PUD_MASK) >> PAGE_SHIFT);
	do {
		VM_BUG_ON(compound_head(page) != head);
		pages[*nr] = page;
		if (PageTail(page))
			get_huge_page_tail(page);
		(*nr)++;
		page++;
		refs++;
	} while (addr += PAGE_SIZE, addr != end);
	get_head_page_multiple(head, refs);

	return 1;
}

static int gup_pud_range(pgd_t pgd, unsigned long addr, unsigned long end,
			int write, struct page **pages, int *nr)
{
	unsigned long next;
	pud_t *pudp;

	pudp = pud_offset(&pgd, addr);
	do {
		pud_t pud = *pudp;

		next = pud_addr_end(addr, end);
		if (pud_none(pud))
			return 0;
		if (unlikely(pud_large(pud))) {
			if (!gup_huge_pud(pud, addr, next, write, pages, nr))
				return 0;
		} else {
			if (!gup_pmd_range(pud, addr, next, write, pages, nr))
				return 0;
		}
	} while (pudp++, addr = next, addr != end);

	return 1;
}

int __get_user_pages_fast(unsigned long start, int nr_pages, int write,
			  struct page **pages)
{
	struct mm_struct *mm = current->mm;
	unsigned long addr, len, end;
	unsigned long next;
	unsigned long flags;
	pgd_t *pgdp;
	int nr = 0;

	start &= PAGE_MASK;
	addr = start;
	len = (unsigned long) nr_pages << PAGE_SHIFT;
	end = start + len;
	if (unlikely(!access_ok(write ? VERIFY_WRITE : VERIFY_READ,
					(void __user *)start, len)))
		return 0;

	local_irq_save(flags);
	pgdp = pgd_offset(mm, addr);
	do {
		pgd_t pgd = *pgdp;

		next = pgd_addr_end(addr, end);
		if (pgd_none(pgd))
			break;
		if (!gup_pud_range(pgd, addr, next, write, pages, &nr))
			break;
	} while (pgdp++, addr = next, addr != end);
	local_irq_restore(flags);

	return nr;
}

/**
 * get_user_pages_fast() - pin user pages in memory
 * @start:	starting user address
 * @nr_pages:	number of pages from start to pin
 * @write:	whether pages will be written to
 * @pages:	array that receives pointers to the pages pinned.
 * 		Should be at least nr_pages long.
 *
 * Attempt to pin user pages in memory without taking mm->mmap_sem.
 * If not successful, it will fall back to taking the lock and
 * calling get_user_pages().
 *
 * Returns number of pages pinned. This may be fewer than the number
 * requested. If nr_pages is 0 or negative, returns 0. If no pages
 * were pinned, returns -errno.
 */
int get_user_pages_fast(unsigned long start, int nr_pages, int write,
			struct page **pages)
{
	struct mm_struct *mm = current->mm;
	unsigned long addr, len, end;
	unsigned long next;
	pgd_t *pgdp;
	int nr = 0;

	start &= PAGE_MASK;
	addr = start;
	len = (unsigned long) nr_pages << PAGE_SHIFT;

	end = start + len;
	if (end < start)
		goto slow_irqon;

#ifdef CONFIG_X86_64
	if (end >> __VIRTUAL_MASK_SHIFT)
		goto slow_irqon;
#endif

	local_irq_disable();
	pgdp = pgd_offset(mm, addr);
	do {
		pgd_t pgd = *pgdp;

		next = pgd_addr_end(addr, end);
		if (pgd_none(pgd))
			goto slow;
		if (!gup_pud_range(pgd, addr, next, write, pages, &nr))
			goto slow;
	} while (pgdp++, addr = next, addr != end);
	local_irq_enable();

	VM_BUG_ON(nr != (end - start) >> PAGE_SHIFT);
	return nr;

	{
		int ret;

slow:
		local_irq_enable();
slow_irqon:
		
		start += nr << PAGE_SHIFT;
		pages += nr;

		down_read(&mm->mmap_sem);
		ret = get_user_pages(current, mm, start,
			(end - start) >> PAGE_SHIFT, write, 0, pages, NULL);
		up_read(&mm->mmap_sem);

		
		if (nr > 0) {
			if (ret < 0)
				ret = nr;
			else
				ret += nr;
		}

		return ret;
	}
}
