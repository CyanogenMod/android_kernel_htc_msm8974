#ifndef _ASM_POWERPC_PGTABLE_PPC64_4K_H
#define _ASM_POWERPC_PGTABLE_PPC64_4K_H
#define PTE_INDEX_SIZE  9
#define PMD_INDEX_SIZE  7
#define PUD_INDEX_SIZE  7
#define PGD_INDEX_SIZE  9

#ifndef __ASSEMBLY__
#define PTE_TABLE_SIZE	(sizeof(pte_t) << PTE_INDEX_SIZE)
#define PMD_TABLE_SIZE	(sizeof(pmd_t) << PMD_INDEX_SIZE)
#define PUD_TABLE_SIZE	(sizeof(pud_t) << PUD_INDEX_SIZE)
#define PGD_TABLE_SIZE	(sizeof(pgd_t) << PGD_INDEX_SIZE)
#endif	

#define PTRS_PER_PTE	(1 << PTE_INDEX_SIZE)
#define PTRS_PER_PMD	(1 << PMD_INDEX_SIZE)
#define PTRS_PER_PUD	(1 << PMD_INDEX_SIZE)
#define PTRS_PER_PGD	(1 << PGD_INDEX_SIZE)

#define PMD_SHIFT	(PAGE_SHIFT + PTE_INDEX_SIZE)
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))

#define MIN_HUGEPTE_SHIFT	PMD_SHIFT

#define PUD_SHIFT	(PMD_SHIFT + PMD_INDEX_SIZE)
#define PUD_SIZE	(1UL << PUD_SHIFT)
#define PUD_MASK	(~(PUD_SIZE-1))

#define PGDIR_SHIFT	(PUD_SHIFT + PUD_INDEX_SIZE)
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

#define PMD_MASKED_BITS		0
#define PUD_MASKED_BITS		0
#define PGD_MASKED_BITS		0



#define pgd_none(pgd)		(!pgd_val(pgd))
#define pgd_bad(pgd)		(pgd_val(pgd) == 0)
#define pgd_present(pgd)	(pgd_val(pgd) != 0)
#define pgd_clear(pgdp)		(pgd_val(*(pgdp)) = 0)
#define pgd_page_vaddr(pgd)	(pgd_val(pgd) & ~PGD_MASKED_BITS)
#define pgd_page(pgd)		virt_to_page(pgd_page_vaddr(pgd))

#define pud_offset(pgdp, addr)	\
  (((pud_t *) pgd_page_vaddr(*(pgdp))) + \
    (((addr) >> PUD_SHIFT) & (PTRS_PER_PUD - 1)))

#define pud_ERROR(e) \
	printk("%s:%d: bad pud %08lx.\n", __FILE__, __LINE__, pud_val(e))

#define remap_4k_pfn(vma, addr, pfn, prot)	\
	remap_pfn_range((vma), (addr), (pfn), PAGE_SIZE, (prot))

#endif 
