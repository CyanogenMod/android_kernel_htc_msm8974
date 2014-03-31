/*
 * Copyright 2002 Andi Kleen, SuSE Labs.
 * Thanks to Ben LaHaise for precious feedback.
 */
#include <linux/highmem.h>
#include <linux/bootmem.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/pfn.h>
#include <linux/percpu.h>
#include <linux/gfp.h>
#include <linux/pci.h>

#include <asm/e820.h>
#include <asm/processor.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/proto.h>
#include <asm/pat.h>

struct cpa_data {
	unsigned long	*vaddr;
	pgprot_t	mask_set;
	pgprot_t	mask_clr;
	int		numpages;
	int		flags;
	unsigned long	pfn;
	unsigned	force_split : 1;
	int		curpage;
	struct page	**pages;
};

static DEFINE_SPINLOCK(cpa_lock);

#define CPA_FLUSHTLB 1
#define CPA_ARRAY 2
#define CPA_PAGES_ARRAY 4

#ifdef CONFIG_PROC_FS
static unsigned long direct_pages_count[PG_LEVEL_NUM];

void update_page_count(int level, unsigned long pages)
{
	
	spin_lock(&pgd_lock);
	direct_pages_count[level] += pages;
	spin_unlock(&pgd_lock);
}

static void split_page_count(int level)
{
	direct_pages_count[level]--;
	direct_pages_count[level - 1] += PTRS_PER_PTE;
}

void arch_report_meminfo(struct seq_file *m)
{
	seq_printf(m, "DirectMap4k:    %8lu kB\n",
			direct_pages_count[PG_LEVEL_4K] << 2);
#if defined(CONFIG_X86_64) || defined(CONFIG_X86_PAE)
	seq_printf(m, "DirectMap2M:    %8lu kB\n",
			direct_pages_count[PG_LEVEL_2M] << 11);
#else
	seq_printf(m, "DirectMap4M:    %8lu kB\n",
			direct_pages_count[PG_LEVEL_2M] << 12);
#endif
#ifdef CONFIG_X86_64
	if (direct_gbpages)
		seq_printf(m, "DirectMap1G:    %8lu kB\n",
			direct_pages_count[PG_LEVEL_1G] << 20);
#endif
}
#else
static inline void split_page_count(int level) { }
#endif

#ifdef CONFIG_X86_64

static inline unsigned long highmap_start_pfn(void)
{
	return __pa(_text) >> PAGE_SHIFT;
}

static inline unsigned long highmap_end_pfn(void)
{
	return __pa(roundup(_brk_end, PMD_SIZE)) >> PAGE_SHIFT;
}

#endif

#ifdef CONFIG_DEBUG_PAGEALLOC
# define debug_pagealloc 1
#else
# define debug_pagealloc 0
#endif

static inline int
within(unsigned long addr, unsigned long start, unsigned long end)
{
	return addr >= start && addr < end;
}


void clflush_cache_range(void *vaddr, unsigned int size)
{
	void *vend = vaddr + size - 1;

	mb();

	for (; vaddr < vend; vaddr += boot_cpu_data.x86_clflush_size)
		clflush(vaddr);
	clflush(vend);

	mb();
}
EXPORT_SYMBOL_GPL(clflush_cache_range);

static void __cpa_flush_all(void *arg)
{
	unsigned long cache = (unsigned long)arg;

	__flush_tlb_all();

	if (cache && boot_cpu_data.x86 >= 4)
		wbinvd();
}

static void cpa_flush_all(unsigned long cache)
{
	BUG_ON(irqs_disabled());

	on_each_cpu(__cpa_flush_all, (void *) cache, 1);
}

static void __cpa_flush_range(void *arg)
{
	__flush_tlb_all();
}

static void cpa_flush_range(unsigned long start, int numpages, int cache)
{
	unsigned int i, level;
	unsigned long addr;

	BUG_ON(irqs_disabled());
	WARN_ON(PAGE_ALIGN(start) != start);

	on_each_cpu(__cpa_flush_range, NULL, 1);

	if (!cache)
		return;

	for (i = 0, addr = start; i < numpages; i++, addr += PAGE_SIZE) {
		pte_t *pte = lookup_address(addr, &level);

		if (pte && (pte_val(*pte) & _PAGE_PRESENT))
			clflush_cache_range((void *) addr, PAGE_SIZE);
	}
}

static void cpa_flush_array(unsigned long *start, int numpages, int cache,
			    int in_flags, struct page **pages)
{
	unsigned int i, level;
	unsigned long do_wbinvd = cache && numpages >= 1024; 

	BUG_ON(irqs_disabled());

	on_each_cpu(__cpa_flush_all, (void *) do_wbinvd, 1);

	if (!cache || do_wbinvd)
		return;

	for (i = 0; i < numpages; i++) {
		unsigned long addr;
		pte_t *pte;

		if (in_flags & CPA_PAGES_ARRAY)
			addr = (unsigned long)page_address(pages[i]);
		else
			addr = start[i];

		pte = lookup_address(addr, &level);

		if (pte && (pte_val(*pte) & _PAGE_PRESENT))
			clflush_cache_range((void *)addr, PAGE_SIZE);
	}
}

static inline pgprot_t static_protections(pgprot_t prot, unsigned long address,
				   unsigned long pfn)
{
	pgprot_t forbidden = __pgprot(0);

#ifdef CONFIG_PCI_BIOS
	if (pcibios_enabled && within(pfn, BIOS_BEGIN >> PAGE_SHIFT, BIOS_END >> PAGE_SHIFT))
		pgprot_val(forbidden) |= _PAGE_NX;
#endif

	if (within(address, (unsigned long)_text, (unsigned long)_etext))
		pgprot_val(forbidden) |= _PAGE_NX;

	if (within(pfn, __pa((unsigned long)__start_rodata) >> PAGE_SHIFT,
		   __pa((unsigned long)__end_rodata) >> PAGE_SHIFT))
		pgprot_val(forbidden) |= _PAGE_RW;

#if defined(CONFIG_X86_64) && defined(CONFIG_DEBUG_RODATA)
	if (kernel_set_to_readonly &&
	    within(address, (unsigned long)_text,
		   (unsigned long)__end_rodata_hpage_align)) {
		unsigned int level;

		if (lookup_address(address, &level) && (level != PG_LEVEL_4K))
			pgprot_val(forbidden) |= _PAGE_RW;
	}
#endif

	prot = __pgprot(pgprot_val(prot) & ~pgprot_val(forbidden));

	return prot;
}

pte_t *lookup_address(unsigned long address, unsigned int *level)
{
	pgd_t *pgd = pgd_offset_k(address);
	pud_t *pud;
	pmd_t *pmd;

	*level = PG_LEVEL_NONE;

	if (pgd_none(*pgd))
		return NULL;

	pud = pud_offset(pgd, address);
	if (pud_none(*pud))
		return NULL;

	*level = PG_LEVEL_1G;
	if (pud_large(*pud) || !pud_present(*pud))
		return (pte_t *)pud;

	pmd = pmd_offset(pud, address);
	if (pmd_none(*pmd))
		return NULL;

	*level = PG_LEVEL_2M;
	if (pmd_large(*pmd) || !pmd_present(*pmd))
		return (pte_t *)pmd;

	*level = PG_LEVEL_4K;

	return pte_offset_kernel(pmd, address);
}
EXPORT_SYMBOL_GPL(lookup_address);

static void __set_pmd_pte(pte_t *kpte, unsigned long address, pte_t pte)
{
	
	set_pte_atomic(kpte, pte);
#ifdef CONFIG_X86_32
	if (!SHARED_KERNEL_PMD) {
		struct page *page;

		list_for_each_entry(page, &pgd_list, lru) {
			pgd_t *pgd;
			pud_t *pud;
			pmd_t *pmd;

			pgd = (pgd_t *)page_address(page) + pgd_index(address);
			pud = pud_offset(pgd, address);
			pmd = pmd_offset(pud, address);
			set_pte_atomic((pte_t *)pmd, pte);
		}
	}
#endif
}

static int
try_preserve_large_page(pte_t *kpte, unsigned long address,
			struct cpa_data *cpa)
{
	unsigned long nextpage_addr, numpages, pmask, psize, addr, pfn;
	pte_t new_pte, old_pte, *tmp;
	pgprot_t old_prot, new_prot, req_prot;
	int i, do_split = 1;
	unsigned int level;

	if (cpa->force_split)
		return 1;

	spin_lock(&pgd_lock);
	tmp = lookup_address(address, &level);
	if (tmp != kpte)
		goto out_unlock;

	switch (level) {
	case PG_LEVEL_2M:
		psize = PMD_PAGE_SIZE;
		pmask = PMD_PAGE_MASK;
		break;
#ifdef CONFIG_X86_64
	case PG_LEVEL_1G:
		psize = PUD_PAGE_SIZE;
		pmask = PUD_PAGE_MASK;
		break;
#endif
	default:
		do_split = -EINVAL;
		goto out_unlock;
	}

	nextpage_addr = (address + psize) & pmask;
	numpages = (nextpage_addr - address) >> PAGE_SHIFT;
	if (numpages < cpa->numpages)
		cpa->numpages = numpages;

	old_pte = *kpte;
	old_prot = new_prot = req_prot = pte_pgprot(old_pte);

	pgprot_val(req_prot) &= ~pgprot_val(cpa->mask_clr);
	pgprot_val(req_prot) |= pgprot_val(cpa->mask_set);

	pfn = pte_pfn(old_pte) + ((address & (psize - 1)) >> PAGE_SHIFT);
	cpa->pfn = pfn;

	new_prot = static_protections(req_prot, address, pfn);

	addr = address & pmask;
	pfn = pte_pfn(old_pte);
	for (i = 0; i < (psize >> PAGE_SHIFT); i++, addr += PAGE_SIZE, pfn++) {
		pgprot_t chk_prot = static_protections(req_prot, addr, pfn);

		if (pgprot_val(chk_prot) != pgprot_val(new_prot))
			goto out_unlock;
	}

	if (pgprot_val(new_prot) == pgprot_val(old_prot)) {
		do_split = 0;
		goto out_unlock;
	}

	if (address == (address & pmask) && cpa->numpages == (psize >> PAGE_SHIFT)) {
		new_pte = pfn_pte(pte_pfn(old_pte), canon_pgprot(new_prot));
		__set_pmd_pte(kpte, address, new_pte);
		cpa->flags |= CPA_FLUSHTLB;
		do_split = 0;
	}

out_unlock:
	spin_unlock(&pgd_lock);

	return do_split;
}

static int split_large_page(pte_t *kpte, unsigned long address)
{
	unsigned long pfn, pfninc = 1;
	unsigned int i, level;
	pte_t *pbase, *tmp;
	pgprot_t ref_prot;
	struct page *base;

	if (!debug_pagealloc)
		spin_unlock(&cpa_lock);
	base = alloc_pages(GFP_KERNEL | __GFP_NOTRACK, 0);
	if (!debug_pagealloc)
		spin_lock(&cpa_lock);
	if (!base)
		return -ENOMEM;

	spin_lock(&pgd_lock);
	tmp = lookup_address(address, &level);
	if (tmp != kpte)
		goto out_unlock;

	pbase = (pte_t *)page_address(base);
	paravirt_alloc_pte(&init_mm, page_to_pfn(base));
	ref_prot = pte_pgprot(pte_clrhuge(*kpte));
	WARN_ON_ONCE(pgprot_val(ref_prot) & _PAGE_PAT_LARGE);

#ifdef CONFIG_X86_64
	if (level == PG_LEVEL_1G) {
		pfninc = PMD_PAGE_SIZE >> PAGE_SHIFT;
		pgprot_val(ref_prot) |= _PAGE_PSE;
	}
#endif

	pfn = pte_pfn(*kpte);
	for (i = 0; i < PTRS_PER_PTE; i++, pfn += pfninc)
		set_pte(&pbase[i], pfn_pte(pfn, ref_prot));

	if (address >= (unsigned long)__va(0) &&
		address < (unsigned long)__va(max_low_pfn_mapped << PAGE_SHIFT))
		split_page_count(level);

#ifdef CONFIG_X86_64
	if (address >= (unsigned long)__va(1UL<<32) &&
		address < (unsigned long)__va(max_pfn_mapped << PAGE_SHIFT))
		split_page_count(level);
#endif

	__set_pmd_pte(kpte, address, mk_pte(base, __pgprot(_KERNPG_TABLE)));

	__flush_tlb_all();

	base = NULL;

out_unlock:
	if (base)
		__free_page(base);
	spin_unlock(&pgd_lock);

	return 0;
}

static int __cpa_process_fault(struct cpa_data *cpa, unsigned long vaddr,
			       int primary)
{
	if (!primary)
		return 0;

	if (within(vaddr, PAGE_OFFSET,
		   PAGE_OFFSET + (max_pfn_mapped << PAGE_SHIFT))) {
		cpa->numpages = 1;
		cpa->pfn = __pa(vaddr) >> PAGE_SHIFT;
		return 0;
	} else {
		WARN(1, KERN_WARNING "CPA: called for zero pte. "
			"vaddr = %lx cpa->vaddr = %lx\n", vaddr,
			*cpa->vaddr);

		return -EFAULT;
	}
}

static int __change_page_attr(struct cpa_data *cpa, int primary)
{
	unsigned long address;
	int do_split, err;
	unsigned int level;
	pte_t *kpte, old_pte;

	if (cpa->flags & CPA_PAGES_ARRAY) {
		struct page *page = cpa->pages[cpa->curpage];
		if (unlikely(PageHighMem(page)))
			return 0;
		address = (unsigned long)page_address(page);
	} else if (cpa->flags & CPA_ARRAY)
		address = cpa->vaddr[cpa->curpage];
	else
		address = *cpa->vaddr;
repeat:
	kpte = lookup_address(address, &level);
	if (!kpte)
		return __cpa_process_fault(cpa, address, primary);

	old_pte = *kpte;
	if (!pte_val(old_pte))
		return __cpa_process_fault(cpa, address, primary);

	if (level == PG_LEVEL_4K) {
		pte_t new_pte;
		pgprot_t new_prot = pte_pgprot(old_pte);
		unsigned long pfn = pte_pfn(old_pte);

		pgprot_val(new_prot) &= ~pgprot_val(cpa->mask_clr);
		pgprot_val(new_prot) |= pgprot_val(cpa->mask_set);

		new_prot = static_protections(new_prot, address, pfn);

		new_pte = pfn_pte(pfn, canon_pgprot(new_prot));
		cpa->pfn = pfn;
		if (pte_val(old_pte) != pte_val(new_pte)) {
			set_pte_atomic(kpte, new_pte);
			cpa->flags |= CPA_FLUSHTLB;
		}
		cpa->numpages = 1;
		return 0;
	}

	do_split = try_preserve_large_page(kpte, address, cpa);
	if (do_split <= 0)
		return do_split;

	err = split_large_page(kpte, address);
	if (!err) {
		flush_tlb_all();
		goto repeat;
	}

	return err;
}

static int __change_page_attr_set_clr(struct cpa_data *cpa, int checkalias);

static int cpa_process_alias(struct cpa_data *cpa)
{
	struct cpa_data alias_cpa;
	unsigned long laddr = (unsigned long)__va(cpa->pfn << PAGE_SHIFT);
	unsigned long vaddr;
	int ret;

	if (cpa->pfn >= max_pfn_mapped)
		return 0;

#ifdef CONFIG_X86_64
	if (cpa->pfn >= max_low_pfn_mapped && cpa->pfn < (1UL<<(32-PAGE_SHIFT)))
		return 0;
#endif
	if (cpa->flags & CPA_PAGES_ARRAY) {
		struct page *page = cpa->pages[cpa->curpage];
		if (unlikely(PageHighMem(page)))
			return 0;
		vaddr = (unsigned long)page_address(page);
	} else if (cpa->flags & CPA_ARRAY)
		vaddr = cpa->vaddr[cpa->curpage];
	else
		vaddr = *cpa->vaddr;

	if (!(within(vaddr, PAGE_OFFSET,
		    PAGE_OFFSET + (max_pfn_mapped << PAGE_SHIFT)))) {

		alias_cpa = *cpa;
		alias_cpa.vaddr = &laddr;
		alias_cpa.flags &= ~(CPA_PAGES_ARRAY | CPA_ARRAY);

		ret = __change_page_attr_set_clr(&alias_cpa, 0);
		if (ret)
			return ret;
	}

#ifdef CONFIG_X86_64
	if (!within(vaddr, (unsigned long)_text, _brk_end) &&
	    within(cpa->pfn, highmap_start_pfn(), highmap_end_pfn())) {
		unsigned long temp_cpa_vaddr = (cpa->pfn << PAGE_SHIFT) +
					       __START_KERNEL_map - phys_base;
		alias_cpa = *cpa;
		alias_cpa.vaddr = &temp_cpa_vaddr;
		alias_cpa.flags &= ~(CPA_PAGES_ARRAY | CPA_ARRAY);

		__change_page_attr_set_clr(&alias_cpa, 0);
	}
#endif

	return 0;
}

static int __change_page_attr_set_clr(struct cpa_data *cpa, int checkalias)
{
	int ret, numpages = cpa->numpages;

	while (numpages) {
		cpa->numpages = numpages;
		
		if (cpa->flags & (CPA_ARRAY | CPA_PAGES_ARRAY))
			cpa->numpages = 1;

		if (!debug_pagealloc)
			spin_lock(&cpa_lock);
		ret = __change_page_attr(cpa, checkalias);
		if (!debug_pagealloc)
			spin_unlock(&cpa_lock);
		if (ret)
			return ret;

		if (checkalias) {
			ret = cpa_process_alias(cpa);
			if (ret)
				return ret;
		}

		BUG_ON(cpa->numpages > numpages);
		numpages -= cpa->numpages;
		if (cpa->flags & (CPA_PAGES_ARRAY | CPA_ARRAY))
			cpa->curpage++;
		else
			*cpa->vaddr += cpa->numpages * PAGE_SIZE;

	}
	return 0;
}

static inline int cache_attr(pgprot_t attr)
{
	return pgprot_val(attr) &
		(_PAGE_PAT | _PAGE_PAT_LARGE | _PAGE_PWT | _PAGE_PCD);
}

static int change_page_attr_set_clr(unsigned long *addr, int numpages,
				    pgprot_t mask_set, pgprot_t mask_clr,
				    int force_split, int in_flag,
				    struct page **pages)
{
	struct cpa_data cpa;
	int ret, cache, checkalias;
	unsigned long baddr = 0;

	mask_set = canon_pgprot(mask_set);
	mask_clr = canon_pgprot(mask_clr);
	if (!pgprot_val(mask_set) && !pgprot_val(mask_clr) && !force_split)
		return 0;

	
	if (in_flag & CPA_ARRAY) {
		int i;
		for (i = 0; i < numpages; i++) {
			if (addr[i] & ~PAGE_MASK) {
				addr[i] &= PAGE_MASK;
				WARN_ON_ONCE(1);
			}
		}
	} else if (!(in_flag & CPA_PAGES_ARRAY)) {
		if (*addr & ~PAGE_MASK) {
			*addr &= PAGE_MASK;
			WARN_ON_ONCE(1);
		}
		baddr = *addr;
	}

	
	kmap_flush_unused();

	vm_unmap_aliases();

	cpa.vaddr = addr;
	cpa.pages = pages;
	cpa.numpages = numpages;
	cpa.mask_set = mask_set;
	cpa.mask_clr = mask_clr;
	cpa.flags = 0;
	cpa.curpage = 0;
	cpa.force_split = force_split;

	if (in_flag & (CPA_ARRAY | CPA_PAGES_ARRAY))
		cpa.flags |= in_flag;

	
	checkalias = (pgprot_val(mask_set) | pgprot_val(mask_clr)) != _PAGE_NX;

	ret = __change_page_attr_set_clr(&cpa, checkalias);

	if (!(cpa.flags & CPA_FLUSHTLB))
		goto out;

	cache = cache_attr(mask_set);

	if (!ret && cpu_has_clflush) {
		if (cpa.flags & (CPA_PAGES_ARRAY | CPA_ARRAY)) {
			cpa_flush_array(addr, numpages, cache,
					cpa.flags, pages);
		} else
			cpa_flush_range(baddr, numpages, cache);
	} else
		cpa_flush_all(cache);

out:
	return ret;
}

static inline int change_page_attr_set(unsigned long *addr, int numpages,
				       pgprot_t mask, int array)
{
	return change_page_attr_set_clr(addr, numpages, mask, __pgprot(0), 0,
		(array ? CPA_ARRAY : 0), NULL);
}

static inline int change_page_attr_clear(unsigned long *addr, int numpages,
					 pgprot_t mask, int array)
{
	return change_page_attr_set_clr(addr, numpages, __pgprot(0), mask, 0,
		(array ? CPA_ARRAY : 0), NULL);
}

static inline int cpa_set_pages_array(struct page **pages, int numpages,
				       pgprot_t mask)
{
	return change_page_attr_set_clr(NULL, numpages, mask, __pgprot(0), 0,
		CPA_PAGES_ARRAY, pages);
}

static inline int cpa_clear_pages_array(struct page **pages, int numpages,
					 pgprot_t mask)
{
	return change_page_attr_set_clr(NULL, numpages, __pgprot(0), mask, 0,
		CPA_PAGES_ARRAY, pages);
}

int _set_memory_uc(unsigned long addr, int numpages)
{
	return change_page_attr_set(&addr, numpages,
				    __pgprot(_PAGE_CACHE_UC_MINUS), 0);
}

int set_memory_uc(unsigned long addr, int numpages)
{
	int ret;

	ret = reserve_memtype(__pa(addr), __pa(addr) + numpages * PAGE_SIZE,
			    _PAGE_CACHE_UC_MINUS, NULL);
	if (ret)
		goto out_err;

	ret = _set_memory_uc(addr, numpages);
	if (ret)
		goto out_free;

	return 0;

out_free:
	free_memtype(__pa(addr), __pa(addr) + numpages * PAGE_SIZE);
out_err:
	return ret;
}
EXPORT_SYMBOL(set_memory_uc);

static int _set_memory_array(unsigned long *addr, int addrinarray,
		unsigned long new_type)
{
	int i, j;
	int ret;

	for (i = 0; i < addrinarray; i++) {
		ret = reserve_memtype(__pa(addr[i]), __pa(addr[i]) + PAGE_SIZE,
					new_type, NULL);
		if (ret)
			goto out_free;
	}

	ret = change_page_attr_set(addr, addrinarray,
				    __pgprot(_PAGE_CACHE_UC_MINUS), 1);

	if (!ret && new_type == _PAGE_CACHE_WC)
		ret = change_page_attr_set_clr(addr, addrinarray,
					       __pgprot(_PAGE_CACHE_WC),
					       __pgprot(_PAGE_CACHE_MASK),
					       0, CPA_ARRAY, NULL);
	if (ret)
		goto out_free;

	return 0;

out_free:
	for (j = 0; j < i; j++)
		free_memtype(__pa(addr[j]), __pa(addr[j]) + PAGE_SIZE);

	return ret;
}

int set_memory_array_uc(unsigned long *addr, int addrinarray)
{
	return _set_memory_array(addr, addrinarray, _PAGE_CACHE_UC_MINUS);
}
EXPORT_SYMBOL(set_memory_array_uc);

int set_memory_array_wc(unsigned long *addr, int addrinarray)
{
	return _set_memory_array(addr, addrinarray, _PAGE_CACHE_WC);
}
EXPORT_SYMBOL(set_memory_array_wc);

int _set_memory_wc(unsigned long addr, int numpages)
{
	int ret;
	unsigned long addr_copy = addr;

	ret = change_page_attr_set(&addr, numpages,
				    __pgprot(_PAGE_CACHE_UC_MINUS), 0);
	if (!ret) {
		ret = change_page_attr_set_clr(&addr_copy, numpages,
					       __pgprot(_PAGE_CACHE_WC),
					       __pgprot(_PAGE_CACHE_MASK),
					       0, 0, NULL);
	}
	return ret;
}

int set_memory_wc(unsigned long addr, int numpages)
{
	int ret;

	if (!pat_enabled)
		return set_memory_uc(addr, numpages);

	ret = reserve_memtype(__pa(addr), __pa(addr) + numpages * PAGE_SIZE,
		_PAGE_CACHE_WC, NULL);
	if (ret)
		goto out_err;

	ret = _set_memory_wc(addr, numpages);
	if (ret)
		goto out_free;

	return 0;

out_free:
	free_memtype(__pa(addr), __pa(addr) + numpages * PAGE_SIZE);
out_err:
	return ret;
}
EXPORT_SYMBOL(set_memory_wc);

int _set_memory_wb(unsigned long addr, int numpages)
{
	return change_page_attr_clear(&addr, numpages,
				      __pgprot(_PAGE_CACHE_MASK), 0);
}

int set_memory_wb(unsigned long addr, int numpages)
{
	int ret;

	ret = _set_memory_wb(addr, numpages);
	if (ret)
		return ret;

	free_memtype(__pa(addr), __pa(addr) + numpages * PAGE_SIZE);
	return 0;
}
EXPORT_SYMBOL(set_memory_wb);

int set_memory_array_wb(unsigned long *addr, int addrinarray)
{
	int i;
	int ret;

	ret = change_page_attr_clear(addr, addrinarray,
				      __pgprot(_PAGE_CACHE_MASK), 1);
	if (ret)
		return ret;

	for (i = 0; i < addrinarray; i++)
		free_memtype(__pa(addr[i]), __pa(addr[i]) + PAGE_SIZE);

	return 0;
}
EXPORT_SYMBOL(set_memory_array_wb);

int set_memory_x(unsigned long addr, int numpages)
{
	if (!(__supported_pte_mask & _PAGE_NX))
		return 0;

	return change_page_attr_clear(&addr, numpages, __pgprot(_PAGE_NX), 0);
}
EXPORT_SYMBOL(set_memory_x);

int set_memory_nx(unsigned long addr, int numpages)
{
	if (!(__supported_pte_mask & _PAGE_NX))
		return 0;

	return change_page_attr_set(&addr, numpages, __pgprot(_PAGE_NX), 0);
}
EXPORT_SYMBOL(set_memory_nx);

int set_memory_ro(unsigned long addr, int numpages)
{
	return change_page_attr_clear(&addr, numpages, __pgprot(_PAGE_RW), 0);
}
EXPORT_SYMBOL_GPL(set_memory_ro);

int set_memory_rw(unsigned long addr, int numpages)
{
	return change_page_attr_set(&addr, numpages, __pgprot(_PAGE_RW), 0);
}
EXPORT_SYMBOL_GPL(set_memory_rw);

int set_memory_np(unsigned long addr, int numpages)
{
	return change_page_attr_clear(&addr, numpages, __pgprot(_PAGE_PRESENT), 0);
}

int set_memory_4k(unsigned long addr, int numpages)
{
	return change_page_attr_set_clr(&addr, numpages, __pgprot(0),
					__pgprot(0), 1, 0, NULL);
}

int set_pages_uc(struct page *page, int numpages)
{
	unsigned long addr = (unsigned long)page_address(page);

	return set_memory_uc(addr, numpages);
}
EXPORT_SYMBOL(set_pages_uc);

static int _set_pages_array(struct page **pages, int addrinarray,
		unsigned long new_type)
{
	unsigned long start;
	unsigned long end;
	int i;
	int free_idx;
	int ret;

	for (i = 0; i < addrinarray; i++) {
		if (PageHighMem(pages[i]))
			continue;
		start = page_to_pfn(pages[i]) << PAGE_SHIFT;
		end = start + PAGE_SIZE;
		if (reserve_memtype(start, end, new_type, NULL))
			goto err_out;
	}

	ret = cpa_set_pages_array(pages, addrinarray,
			__pgprot(_PAGE_CACHE_UC_MINUS));
	if (!ret && new_type == _PAGE_CACHE_WC)
		ret = change_page_attr_set_clr(NULL, addrinarray,
					       __pgprot(_PAGE_CACHE_WC),
					       __pgprot(_PAGE_CACHE_MASK),
					       0, CPA_PAGES_ARRAY, pages);
	if (ret)
		goto err_out;
	return 0; 
err_out:
	free_idx = i;
	for (i = 0; i < free_idx; i++) {
		if (PageHighMem(pages[i]))
			continue;
		start = page_to_pfn(pages[i]) << PAGE_SHIFT;
		end = start + PAGE_SIZE;
		free_memtype(start, end);
	}
	return -EINVAL;
}

int set_pages_array_uc(struct page **pages, int addrinarray)
{
	return _set_pages_array(pages, addrinarray, _PAGE_CACHE_UC_MINUS);
}
EXPORT_SYMBOL(set_pages_array_uc);

int set_pages_array_wc(struct page **pages, int addrinarray)
{
	return _set_pages_array(pages, addrinarray, _PAGE_CACHE_WC);
}
EXPORT_SYMBOL(set_pages_array_wc);

int set_pages_wb(struct page *page, int numpages)
{
	unsigned long addr = (unsigned long)page_address(page);

	return set_memory_wb(addr, numpages);
}
EXPORT_SYMBOL(set_pages_wb);

int set_pages_array_wb(struct page **pages, int addrinarray)
{
	int retval;
	unsigned long start;
	unsigned long end;
	int i;

	retval = cpa_clear_pages_array(pages, addrinarray,
			__pgprot(_PAGE_CACHE_MASK));
	if (retval)
		return retval;

	for (i = 0; i < addrinarray; i++) {
		if (PageHighMem(pages[i]))
			continue;
		start = page_to_pfn(pages[i]) << PAGE_SHIFT;
		end = start + PAGE_SIZE;
		free_memtype(start, end);
	}

	return 0;
}
EXPORT_SYMBOL(set_pages_array_wb);

int set_pages_x(struct page *page, int numpages)
{
	unsigned long addr = (unsigned long)page_address(page);

	return set_memory_x(addr, numpages);
}
EXPORT_SYMBOL(set_pages_x);

int set_pages_nx(struct page *page, int numpages)
{
	unsigned long addr = (unsigned long)page_address(page);

	return set_memory_nx(addr, numpages);
}
EXPORT_SYMBOL(set_pages_nx);

int set_pages_ro(struct page *page, int numpages)
{
	unsigned long addr = (unsigned long)page_address(page);

	return set_memory_ro(addr, numpages);
}

int set_pages_rw(struct page *page, int numpages)
{
	unsigned long addr = (unsigned long)page_address(page);

	return set_memory_rw(addr, numpages);
}

#ifdef CONFIG_DEBUG_PAGEALLOC

static int __set_pages_p(struct page *page, int numpages)
{
	unsigned long tempaddr = (unsigned long) page_address(page);
	struct cpa_data cpa = { .vaddr = &tempaddr,
				.numpages = numpages,
				.mask_set = __pgprot(_PAGE_PRESENT | _PAGE_RW),
				.mask_clr = __pgprot(0),
				.flags = 0};

	return __change_page_attr_set_clr(&cpa, 0);
}

static int __set_pages_np(struct page *page, int numpages)
{
	unsigned long tempaddr = (unsigned long) page_address(page);
	struct cpa_data cpa = { .vaddr = &tempaddr,
				.numpages = numpages,
				.mask_set = __pgprot(0),
				.mask_clr = __pgprot(_PAGE_PRESENT | _PAGE_RW),
				.flags = 0};

	return __change_page_attr_set_clr(&cpa, 0);
}

void kernel_map_pages(struct page *page, int numpages, int enable)
{
	if (PageHighMem(page))
		return;
	if (!enable) {
		debug_check_no_locks_freed(page_address(page),
					   numpages * PAGE_SIZE);
	}

	if (enable)
		__set_pages_p(page, numpages);
	else
		__set_pages_np(page, numpages);

	__flush_tlb_all();
}

#ifdef CONFIG_HIBERNATION

bool kernel_page_present(struct page *page)
{
	unsigned int level;
	pte_t *pte;

	if (PageHighMem(page))
		return false;

	pte = lookup_address((unsigned long)page_address(page), &level);
	return (pte_val(*pte) & _PAGE_PRESENT);
}

#endif 

#endif 

#ifdef CONFIG_CPA_DEBUG
#include "pageattr-test.c"
#endif
