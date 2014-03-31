/*
 * Hibernation support specific for i386 - temporary page tables
 *
 * Distribute under GPLv2
 *
 * Copyright (c) 2006 Rafael J. Wysocki <rjw@sisk.pl>
 */

#include <linux/gfp.h>
#include <linux/suspend.h>
#include <linux/bootmem.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/mmzone.h>

extern int restore_image(void);

extern const void __nosave_begin, __nosave_end;

pgd_t *resume_pg_dir;


static pmd_t *resume_one_md_table_init(pgd_t *pgd)
{
	pud_t *pud;
	pmd_t *pmd_table;

#ifdef CONFIG_X86_PAE
	pmd_table = (pmd_t *)get_safe_page(GFP_ATOMIC);
	if (!pmd_table)
		return NULL;

	set_pgd(pgd, __pgd(__pa(pmd_table) | _PAGE_PRESENT));
	pud = pud_offset(pgd, 0);

	BUG_ON(pmd_table != pmd_offset(pud, 0));
#else
	pud = pud_offset(pgd, 0);
	pmd_table = pmd_offset(pud, 0);
#endif

	return pmd_table;
}

static pte_t *resume_one_page_table_init(pmd_t *pmd)
{
	if (pmd_none(*pmd)) {
		pte_t *page_table = (pte_t *)get_safe_page(GFP_ATOMIC);
		if (!page_table)
			return NULL;

		set_pmd(pmd, __pmd(__pa(page_table) | _PAGE_TABLE));

		BUG_ON(page_table != pte_offset_kernel(pmd, 0));

		return page_table;
	}

	return pte_offset_kernel(pmd, 0);
}

static int resume_physical_mapping_init(pgd_t *pgd_base)
{
	unsigned long pfn;
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;
	int pgd_idx, pmd_idx;

	pgd_idx = pgd_index(PAGE_OFFSET);
	pgd = pgd_base + pgd_idx;
	pfn = 0;

	for (; pgd_idx < PTRS_PER_PGD; pgd++, pgd_idx++) {
		pmd = resume_one_md_table_init(pgd);
		if (!pmd)
			return -ENOMEM;

		if (pfn >= max_low_pfn)
			continue;

		for (pmd_idx = 0; pmd_idx < PTRS_PER_PMD; pmd++, pmd_idx++) {
			if (pfn >= max_low_pfn)
				break;

			if (cpu_has_pse) {
				set_pmd(pmd, pfn_pmd(pfn, PAGE_KERNEL_LARGE_EXEC));
				pfn += PTRS_PER_PTE;
			} else {
				pte_t *max_pte;

				pte = resume_one_page_table_init(pmd);
				if (!pte)
					return -ENOMEM;

				max_pte = pte + PTRS_PER_PTE;
				for (; pte < max_pte; pte++, pfn++) {
					if (pfn >= max_low_pfn)
						break;

					set_pte(pte, pfn_pte(pfn, PAGE_KERNEL_EXEC));
				}
			}
		}
	}

	resume_map_numa_kva(pgd_base);

	return 0;
}

static inline void resume_init_first_level_page_table(pgd_t *pg_dir)
{
#ifdef CONFIG_X86_PAE
	int i;

	
	for (i = 0; i < PTRS_PER_PGD; i++)
		set_pgd(pg_dir + i,
			__pgd(__pa(empty_zero_page) | _PAGE_PRESENT));
#endif
}

int swsusp_arch_resume(void)
{
	int error;

	resume_pg_dir = (pgd_t *)get_safe_page(GFP_ATOMIC);
	if (!resume_pg_dir)
		return -ENOMEM;

	resume_init_first_level_page_table(resume_pg_dir);
	error = resume_physical_mapping_init(resume_pg_dir);
	if (error)
		return error;

	
	restore_image();
	return 0;
}


int pfn_is_nosave(unsigned long pfn)
{
	unsigned long nosave_begin_pfn = __pa_symbol(&__nosave_begin) >> PAGE_SHIFT;
	unsigned long nosave_end_pfn = PAGE_ALIGN(__pa_symbol(&__nosave_end)) >> PAGE_SHIFT;
	return (pfn >= nosave_begin_pfn) && (pfn < nosave_end_pfn);
}
