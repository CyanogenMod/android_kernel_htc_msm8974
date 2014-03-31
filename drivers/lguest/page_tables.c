
/* Copyright (C) Rusty Russell IBM Corporation 2006.
 * GPL v2 and any later version */
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <linux/percpu.h>
#include <asm/tlbflush.h>
#include <asm/uaccess.h>
#include "lg.h"



#define SWITCHER_PGD_INDEX (PTRS_PER_PGD - 1)

#ifdef CONFIG_X86_PAE
#define SWITCHER_PMD_INDEX 	(PTRS_PER_PMD - 1)
#define RESERVE_MEM 		2U
#define CHECK_GPGD_MASK		_PAGE_PRESENT
#else
#define RESERVE_MEM 		4U
#define CHECK_GPGD_MASK		_PAGE_TABLE
#endif

static DEFINE_PER_CPU(pte_t *, switcher_pte_pages);
#define switcher_pte_page(cpu) per_cpu(switcher_pte_pages, cpu)

static pgd_t *spgd_addr(struct lg_cpu *cpu, u32 i, unsigned long vaddr)
{
	unsigned int index = pgd_index(vaddr);

#ifndef CONFIG_X86_PAE
	
	if (index >= SWITCHER_PGD_INDEX) {
		kill_guest(cpu, "attempt to access switcher pages");
		index = 0;
	}
#endif
	
	return &cpu->lg->pgdirs[i].pgdir[index];
}

#ifdef CONFIG_X86_PAE
static pmd_t *spmd_addr(struct lg_cpu *cpu, pgd_t spgd, unsigned long vaddr)
{
	unsigned int index = pmd_index(vaddr);
	pmd_t *page;

	
	if (pgd_index(vaddr) == SWITCHER_PGD_INDEX &&
					index >= SWITCHER_PMD_INDEX) {
		kill_guest(cpu, "attempt to access switcher pages");
		index = 0;
	}

	
	BUG_ON(!(pgd_flags(spgd) & _PAGE_PRESENT));
	page = __va(pgd_pfn(spgd) << PAGE_SHIFT);

	return &page[index];
}
#endif

static pte_t *spte_addr(struct lg_cpu *cpu, pgd_t spgd, unsigned long vaddr)
{
#ifdef CONFIG_X86_PAE
	pmd_t *pmd = spmd_addr(cpu, spgd, vaddr);
	pte_t *page = __va(pmd_pfn(*pmd) << PAGE_SHIFT);

	
	BUG_ON(!(pmd_flags(*pmd) & _PAGE_PRESENT));
#else
	pte_t *page = __va(pgd_pfn(spgd) << PAGE_SHIFT);
	
	BUG_ON(!(pgd_flags(spgd) & _PAGE_PRESENT));
#endif

	return &page[pte_index(vaddr)];
}

static unsigned long gpgd_addr(struct lg_cpu *cpu, unsigned long vaddr)
{
	unsigned int index = vaddr >> (PGDIR_SHIFT);
	return cpu->lg->pgdirs[cpu->cpu_pgd].gpgdir + index * sizeof(pgd_t);
}

#ifdef CONFIG_X86_PAE
static unsigned long gpmd_addr(pgd_t gpgd, unsigned long vaddr)
{
	unsigned long gpage = pgd_pfn(gpgd) << PAGE_SHIFT;
	BUG_ON(!(pgd_flags(gpgd) & _PAGE_PRESENT));
	return gpage + pmd_index(vaddr) * sizeof(pmd_t);
}

static unsigned long gpte_addr(struct lg_cpu *cpu,
			       pmd_t gpmd, unsigned long vaddr)
{
	unsigned long gpage = pmd_pfn(gpmd) << PAGE_SHIFT;

	BUG_ON(!(pmd_flags(gpmd) & _PAGE_PRESENT));
	return gpage + pte_index(vaddr) * sizeof(pte_t);
}
#else
static unsigned long gpte_addr(struct lg_cpu *cpu,
				pgd_t gpgd, unsigned long vaddr)
{
	unsigned long gpage = pgd_pfn(gpgd) << PAGE_SHIFT;

	BUG_ON(!(pgd_flags(gpgd) & _PAGE_PRESENT));
	return gpage + pte_index(vaddr) * sizeof(pte_t);
}
#endif


static unsigned long get_pfn(unsigned long virtpfn, int write)
{
	struct page *page;

	
	if (get_user_pages_fast(virtpfn << PAGE_SHIFT, 1, write, &page) == 1)
		return page_to_pfn(page);

	
	return -1UL;
}

static pte_t gpte_to_spte(struct lg_cpu *cpu, pte_t gpte, int write)
{
	unsigned long pfn, base, flags;

	flags = (pte_flags(gpte) & ~_PAGE_GLOBAL);

	
	base = (unsigned long)cpu->lg->mem_base / PAGE_SIZE;

	pfn = get_pfn(base + pte_pfn(gpte), write);
	if (pfn == -1UL) {
		kill_guest(cpu, "failed to get page %lu", pte_pfn(gpte));
		flags = 0;
	}
	
	return pfn_pte(pfn, __pgprot(flags));
}

static void release_pte(pte_t pte)
{
	if (pte_flags(pte) & _PAGE_PRESENT)
		put_page(pte_page(pte));
}

static void check_gpte(struct lg_cpu *cpu, pte_t gpte)
{
	if ((pte_flags(gpte) & _PAGE_PSE) ||
	    pte_pfn(gpte) >= cpu->lg->pfn_limit)
		kill_guest(cpu, "bad page table entry");
}

static void check_gpgd(struct lg_cpu *cpu, pgd_t gpgd)
{
	if ((pgd_flags(gpgd) & ~CHECK_GPGD_MASK) ||
	   (pgd_pfn(gpgd) >= cpu->lg->pfn_limit))
		kill_guest(cpu, "bad page directory entry");
}

#ifdef CONFIG_X86_PAE
static void check_gpmd(struct lg_cpu *cpu, pmd_t gpmd)
{
	if ((pmd_flags(gpmd) & ~_PAGE_TABLE) ||
	   (pmd_pfn(gpmd) >= cpu->lg->pfn_limit))
		kill_guest(cpu, "bad page middle directory entry");
}
#endif

bool demand_page(struct lg_cpu *cpu, unsigned long vaddr, int errcode)
{
	pgd_t gpgd;
	pgd_t *spgd;
	unsigned long gpte_ptr;
	pte_t gpte;
	pte_t *spte;

	
#ifdef CONFIG_X86_PAE
	pmd_t *spmd;
	pmd_t gpmd;
#endif

	
	if (unlikely(cpu->linear_pages)) {
		
		gpgd = __pgd(CHECK_GPGD_MASK);
	} else {
		gpgd = lgread(cpu, gpgd_addr(cpu, vaddr), pgd_t);
		
		if (!(pgd_flags(gpgd) & _PAGE_PRESENT))
			return false;
	}

	
	spgd = spgd_addr(cpu, cpu->cpu_pgd, vaddr);
	if (!(pgd_flags(*spgd) & _PAGE_PRESENT)) {
		
		unsigned long ptepage = get_zeroed_page(GFP_KERNEL);
		if (!ptepage) {
			kill_guest(cpu, "out of memory allocating pte page");
			return false;
		}
		
		check_gpgd(cpu, gpgd);
		set_pgd(spgd, __pgd(__pa(ptepage) | pgd_flags(gpgd)));
	}

#ifdef CONFIG_X86_PAE
	if (unlikely(cpu->linear_pages)) {
		
		gpmd = __pmd(_PAGE_TABLE);
	} else {
		gpmd = lgread(cpu, gpmd_addr(gpgd, vaddr), pmd_t);
		
		if (!(pmd_flags(gpmd) & _PAGE_PRESENT))
			return false;
	}

	
	spmd = spmd_addr(cpu, *spgd, vaddr);

	if (!(pmd_flags(*spmd) & _PAGE_PRESENT)) {
		
		unsigned long ptepage = get_zeroed_page(GFP_KERNEL);

		if (!ptepage) {
			kill_guest(cpu, "out of memory allocating pte page");
			return false;
		}

		
		check_gpmd(cpu, gpmd);

		set_pmd(spmd, __pmd(__pa(ptepage) | pmd_flags(gpmd)));
	}

	gpte_ptr = gpte_addr(cpu, gpmd, vaddr);
#else
	gpte_ptr = gpte_addr(cpu, gpgd, vaddr);
#endif

	if (unlikely(cpu->linear_pages)) {
		
		gpte = __pte((vaddr & PAGE_MASK) | _PAGE_RW | _PAGE_PRESENT);
	} else {
		
		gpte = lgread(cpu, gpte_ptr, pte_t);
	}

	
	if (!(pte_flags(gpte) & _PAGE_PRESENT))
		return false;

	if ((errcode & 2) && !(pte_flags(gpte) & _PAGE_RW))
		return false;

	
	if ((errcode & 4) && !(pte_flags(gpte) & _PAGE_USER))
		return false;

	check_gpte(cpu, gpte);

	
	gpte = pte_mkyoung(gpte);
	if (errcode & 2)
		gpte = pte_mkdirty(gpte);

	
	spte = spte_addr(cpu, *spgd, vaddr);

	release_pte(*spte);

	if (pte_dirty(gpte))
		*spte = gpte_to_spte(cpu, gpte, 1);
	else
		set_pte(spte, gpte_to_spte(cpu, pte_wrprotect(gpte), 0));

	if (likely(!cpu->linear_pages))
		lgwrite(cpu, gpte_ptr, pte_t, gpte);

	return true;
}

static bool page_writable(struct lg_cpu *cpu, unsigned long vaddr)
{
	pgd_t *spgd;
	unsigned long flags;

#ifdef CONFIG_X86_PAE
	pmd_t *spmd;
#endif
	
	spgd = spgd_addr(cpu, cpu->cpu_pgd, vaddr);
	if (!(pgd_flags(*spgd) & _PAGE_PRESENT))
		return false;

#ifdef CONFIG_X86_PAE
	spmd = spmd_addr(cpu, *spgd, vaddr);
	if (!(pmd_flags(*spmd) & _PAGE_PRESENT))
		return false;
#endif

	flags = pte_flags(*(spte_addr(cpu, *spgd, vaddr)));

	return (flags & (_PAGE_PRESENT|_PAGE_RW)) == (_PAGE_PRESENT|_PAGE_RW);
}

void pin_page(struct lg_cpu *cpu, unsigned long vaddr)
{
	if (!page_writable(cpu, vaddr) && !demand_page(cpu, vaddr, 2))
		kill_guest(cpu, "bad stack page %#lx", vaddr);
}

#ifdef CONFIG_X86_PAE
static void release_pmd(pmd_t *spmd)
{
	
	if (pmd_flags(*spmd) & _PAGE_PRESENT) {
		unsigned int i;
		pte_t *ptepage = __va(pmd_pfn(*spmd) << PAGE_SHIFT);
		
		for (i = 0; i < PTRS_PER_PTE; i++)
			release_pte(ptepage[i]);
		
		free_page((long)ptepage);
		
		set_pmd(spmd, __pmd(0));
	}
}

static void release_pgd(pgd_t *spgd)
{
	
	if (pgd_flags(*spgd) & _PAGE_PRESENT) {
		unsigned int i;
		pmd_t *pmdpage = __va(pgd_pfn(*spgd) << PAGE_SHIFT);

		for (i = 0; i < PTRS_PER_PMD; i++)
			release_pmd(&pmdpage[i]);

		
		free_page((long)pmdpage);
		
		set_pgd(spgd, __pgd(0));
	}
}

#else 
static void release_pgd(pgd_t *spgd)
{
	
	if (pgd_flags(*spgd) & _PAGE_PRESENT) {
		unsigned int i;
		pte_t *ptepage = __va(pgd_pfn(*spgd) << PAGE_SHIFT);
		
		for (i = 0; i < PTRS_PER_PTE; i++)
			release_pte(ptepage[i]);
		
		free_page((long)ptepage);
		
		*spgd = __pgd(0);
	}
}
#endif

static void flush_user_mappings(struct lguest *lg, int idx)
{
	unsigned int i;
	
	for (i = 0; i < pgd_index(lg->kernel_address); i++)
		release_pgd(lg->pgdirs[idx].pgdir + i);
}

void guest_pagetable_flush_user(struct lg_cpu *cpu)
{
	
	flush_user_mappings(cpu->lg, cpu->cpu_pgd);
}

unsigned long guest_pa(struct lg_cpu *cpu, unsigned long vaddr)
{
	pgd_t gpgd;
	pte_t gpte;
#ifdef CONFIG_X86_PAE
	pmd_t gpmd;
#endif

	
	if (unlikely(cpu->linear_pages))
		return vaddr;

	
	gpgd = lgread(cpu, gpgd_addr(cpu, vaddr), pgd_t);
	
	if (!(pgd_flags(gpgd) & _PAGE_PRESENT)) {
		kill_guest(cpu, "Bad address %#lx", vaddr);
		return -1UL;
	}

#ifdef CONFIG_X86_PAE
	gpmd = lgread(cpu, gpmd_addr(gpgd, vaddr), pmd_t);
	if (!(pmd_flags(gpmd) & _PAGE_PRESENT))
		kill_guest(cpu, "Bad address %#lx", vaddr);
	gpte = lgread(cpu, gpte_addr(cpu, gpmd, vaddr), pte_t);
#else
	gpte = lgread(cpu, gpte_addr(cpu, gpgd, vaddr), pte_t);
#endif
	if (!(pte_flags(gpte) & _PAGE_PRESENT))
		kill_guest(cpu, "Bad address %#lx", vaddr);

	return pte_pfn(gpte) * PAGE_SIZE | (vaddr & ~PAGE_MASK);
}

static unsigned int find_pgdir(struct lguest *lg, unsigned long pgtable)
{
	unsigned int i;
	for (i = 0; i < ARRAY_SIZE(lg->pgdirs); i++)
		if (lg->pgdirs[i].pgdir && lg->pgdirs[i].gpgdir == pgtable)
			break;
	return i;
}

static unsigned int new_pgdir(struct lg_cpu *cpu,
			      unsigned long gpgdir,
			      int *blank_pgdir)
{
	unsigned int next;
#ifdef CONFIG_X86_PAE
	pmd_t *pmd_table;
#endif

	next = random32() % ARRAY_SIZE(cpu->lg->pgdirs);
	
	if (!cpu->lg->pgdirs[next].pgdir) {
		cpu->lg->pgdirs[next].pgdir =
					(pgd_t *)get_zeroed_page(GFP_KERNEL);
		
		if (!cpu->lg->pgdirs[next].pgdir)
			next = cpu->cpu_pgd;
		else {
#ifdef CONFIG_X86_PAE
			pmd_table = (pmd_t *)get_zeroed_page(GFP_KERNEL);
			if (!pmd_table) {
				free_page((long)cpu->lg->pgdirs[next].pgdir);
				set_pgd(cpu->lg->pgdirs[next].pgdir, __pgd(0));
				next = cpu->cpu_pgd;
			} else {
				set_pgd(cpu->lg->pgdirs[next].pgdir +
					SWITCHER_PGD_INDEX,
					__pgd(__pa(pmd_table) | _PAGE_PRESENT));
				*blank_pgdir = 1;
			}
#else
			*blank_pgdir = 1;
#endif
		}
	}
	
	cpu->lg->pgdirs[next].gpgdir = gpgdir;
	
	flush_user_mappings(cpu->lg, next);

	return next;
}

static void release_all_pagetables(struct lguest *lg)
{
	unsigned int i, j;

	
	for (i = 0; i < ARRAY_SIZE(lg->pgdirs); i++)
		if (lg->pgdirs[i].pgdir) {
#ifdef CONFIG_X86_PAE
			pgd_t *spgd;
			pmd_t *pmdpage;
			unsigned int k;

			
			spgd = lg->pgdirs[i].pgdir + SWITCHER_PGD_INDEX;
			pmdpage = __va(pgd_pfn(*spgd) << PAGE_SHIFT);

			for (k = 0; k < SWITCHER_PMD_INDEX; k++)
				release_pmd(&pmdpage[k]);
#endif
			
			for (j = 0; j < SWITCHER_PGD_INDEX; j++)
				release_pgd(lg->pgdirs[i].pgdir + j);
		}
}

void guest_pagetable_clear_all(struct lg_cpu *cpu)
{
	release_all_pagetables(cpu->lg);
	
	pin_stack_pages(cpu);
}

void guest_new_pagetable(struct lg_cpu *cpu, unsigned long pgtable)
{
	int newpgdir, repin = 0;

	if (unlikely(cpu->linear_pages)) {
		release_all_pagetables(cpu->lg);
		cpu->linear_pages = false;
		
		newpgdir = ARRAY_SIZE(cpu->lg->pgdirs);
	} else {
		
		newpgdir = find_pgdir(cpu->lg, pgtable);
	}

	if (newpgdir == ARRAY_SIZE(cpu->lg->pgdirs))
		newpgdir = new_pgdir(cpu, pgtable, &repin);
	
	cpu->cpu_pgd = newpgdir;
	
	if (repin)
		pin_stack_pages(cpu);
}


/*H:420
 * This is the routine which actually sets the page table entry for then
 * "idx"'th shadow page table.
 *
 * Normally, we can just throw out the old entry and replace it with 0: if they
 * use it demand_page() will put the new entry in.  We need to do this anyway:
 * The Guest expects _PAGE_ACCESSED to be set on its PTE the first time a page
 * is read from, and _PAGE_DIRTY when it's written to.
 *
 * But Avi Kivity pointed out that most Operating Systems (Linux included) set
 * these bits on PTEs immediately anyway.  This is done to save the CPU from
 * having to update them, but it helps us the same way: if they set
 * _PAGE_ACCESSED then we can put a read-only PTE entry in immediately, and if
 * they set _PAGE_DIRTY then we can put a writable PTE entry in immediately.
 */
static void do_set_pte(struct lg_cpu *cpu, int idx,
		       unsigned long vaddr, pte_t gpte)
{
	
	pgd_t *spgd = spgd_addr(cpu, idx, vaddr);
#ifdef CONFIG_X86_PAE
	pmd_t *spmd;
#endif

	
	if (pgd_flags(*spgd) & _PAGE_PRESENT) {
#ifdef CONFIG_X86_PAE
		spmd = spmd_addr(cpu, *spgd, vaddr);
		if (pmd_flags(*spmd) & _PAGE_PRESENT) {
#endif
			
			pte_t *spte = spte_addr(cpu, *spgd, vaddr);
			release_pte(*spte);

			if (pte_flags(gpte) & (_PAGE_DIRTY | _PAGE_ACCESSED)) {
				check_gpte(cpu, gpte);
				set_pte(spte,
					gpte_to_spte(cpu, gpte,
						pte_flags(gpte) & _PAGE_DIRTY));
			} else {
				set_pte(spte, __pte(0));
			}
#ifdef CONFIG_X86_PAE
		}
#endif
	}
}

void guest_set_pte(struct lg_cpu *cpu,
		   unsigned long gpgdir, unsigned long vaddr, pte_t gpte)
{
	if (vaddr >= cpu->lg->kernel_address) {
		unsigned int i;
		for (i = 0; i < ARRAY_SIZE(cpu->lg->pgdirs); i++)
			if (cpu->lg->pgdirs[i].pgdir)
				do_set_pte(cpu, i, vaddr, gpte);
	} else {
		
		int pgdir = find_pgdir(cpu->lg, gpgdir);
		if (pgdir != ARRAY_SIZE(cpu->lg->pgdirs))
			
			do_set_pte(cpu, pgdir, vaddr, gpte);
	}
}

void guest_set_pgd(struct lguest *lg, unsigned long gpgdir, u32 idx)
{
	int pgdir;

	if (idx >= SWITCHER_PGD_INDEX)
		return;

	
	pgdir = find_pgdir(lg, gpgdir);
	if (pgdir < ARRAY_SIZE(lg->pgdirs))
		
		release_pgd(lg->pgdirs[pgdir].pgdir + idx);
}

#ifdef CONFIG_X86_PAE
void guest_set_pmd(struct lguest *lg, unsigned long pmdp, u32 idx)
{
	guest_pagetable_clear_all(&lg->cpus[0]);
}
#endif

int init_guest_pagetable(struct lguest *lg)
{
	struct lg_cpu *cpu = &lg->cpus[0];
	int allocated = 0;

	
	cpu->cpu_pgd = new_pgdir(cpu, 0, &allocated);
	if (!allocated)
		return -ENOMEM;

	
	cpu->linear_pages = true;
	return 0;
}

void page_table_guest_data_init(struct lg_cpu *cpu)
{
	
	if (get_user(cpu->lg->kernel_address,
		&cpu->lg->lguest_data->kernel_address)
		|| put_user(RESERVE_MEM * 1024 * 1024,
			    &cpu->lg->lguest_data->reserve_mem)) {
		kill_guest(cpu, "bad guest page %p", cpu->lg->lguest_data);
		return;
	}

#ifdef CONFIG_X86_PAE
	if (pgd_index(cpu->lg->kernel_address) == SWITCHER_PGD_INDEX &&
		pmd_index(cpu->lg->kernel_address) == SWITCHER_PMD_INDEX)
#else
	if (pgd_index(cpu->lg->kernel_address) >= SWITCHER_PGD_INDEX)
#endif
		kill_guest(cpu, "bad kernel address %#lx",
				 cpu->lg->kernel_address);
}

void free_guest_pagetable(struct lguest *lg)
{
	unsigned int i;

	
	release_all_pagetables(lg);
	
	for (i = 0; i < ARRAY_SIZE(lg->pgdirs); i++)
		free_page((long)lg->pgdirs[i].pgdir);
}

void map_switcher_in_guest(struct lg_cpu *cpu, struct lguest_pages *pages)
{
	pte_t *switcher_pte_page = __this_cpu_read(switcher_pte_pages);
	pte_t regs_pte;

#ifdef CONFIG_X86_PAE
	pmd_t switcher_pmd;
	pmd_t *pmd_table;

	switcher_pmd = pfn_pmd(__pa(switcher_pte_page) >> PAGE_SHIFT,
			       PAGE_KERNEL_EXEC);

	pmd_table = __va(pgd_pfn(cpu->lg->
			pgdirs[cpu->cpu_pgd].pgdir[SWITCHER_PGD_INDEX])
								<< PAGE_SHIFT);
	
	set_pmd(&pmd_table[SWITCHER_PMD_INDEX], switcher_pmd);
#else
	pgd_t switcher_pgd;

	switcher_pgd = __pgd(__pa(switcher_pte_page) | __PAGE_KERNEL_EXEC);

	cpu->lg->pgdirs[cpu->cpu_pgd].pgdir[SWITCHER_PGD_INDEX] = switcher_pgd;

#endif
	regs_pte = pfn_pte(__pa(cpu->regs_page) >> PAGE_SHIFT, PAGE_KERNEL);
	set_pte(&switcher_pte_page[pte_index((unsigned long)pages)], regs_pte);
}

static void free_switcher_pte_pages(void)
{
	unsigned int i;

	for_each_possible_cpu(i)
		free_page((long)switcher_pte_page(i));
}

static __init void populate_switcher_pte_page(unsigned int cpu,
					      struct page *switcher_page[],
					      unsigned int pages)
{
	unsigned int i;
	pte_t *pte = switcher_pte_page(cpu);

	
	for (i = 0; i < pages; i++) {
		set_pte(&pte[i], mk_pte(switcher_page[i],
				__pgprot(_PAGE_PRESENT|_PAGE_ACCESSED)));
	}

	
	i = pages + cpu*2;

	
	set_pte(&pte[i], pfn_pte(page_to_pfn(switcher_page[i]),
			 __pgprot(_PAGE_PRESENT|_PAGE_ACCESSED|_PAGE_RW)));

	set_pte(&pte[i+1], pfn_pte(page_to_pfn(switcher_page[i+1]),
			   __pgprot(_PAGE_PRESENT|_PAGE_ACCESSED)));
}


__init int init_pagetables(struct page **switcher_page, unsigned int pages)
{
	unsigned int i;

	for_each_possible_cpu(i) {
		switcher_pte_page(i) = (pte_t *)get_zeroed_page(GFP_KERNEL);
		if (!switcher_pte_page(i)) {
			free_switcher_pte_pages();
			return -ENOMEM;
		}
		populate_switcher_pte_page(i, switcher_page, pages);
	}
	return 0;
}

void free_pagetables(void)
{
	free_switcher_pte_pages();
}
