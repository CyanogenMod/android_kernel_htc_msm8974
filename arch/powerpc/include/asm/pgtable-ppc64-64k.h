#ifndef _ASM_POWERPC_PGTABLE_PPC64_64K_H
#define _ASM_POWERPC_PGTABLE_PPC64_64K_H

#include <asm-generic/pgtable-nopud.h>


#define PTE_INDEX_SIZE  12
#define PMD_INDEX_SIZE  12
#define PUD_INDEX_SIZE	0
#define PGD_INDEX_SIZE  4

#ifndef __ASSEMBLY__
#define PTE_TABLE_SIZE	(sizeof(real_pte_t) << PTE_INDEX_SIZE)
#define PMD_TABLE_SIZE	(sizeof(pmd_t) << PMD_INDEX_SIZE)
#define PGD_TABLE_SIZE	(sizeof(pgd_t) << PGD_INDEX_SIZE)
#endif	

#define PTRS_PER_PTE	(1 << PTE_INDEX_SIZE)
#define PTRS_PER_PMD	(1 << PMD_INDEX_SIZE)
#define PTRS_PER_PGD	(1 << PGD_INDEX_SIZE)

#define MIN_HUGEPTE_SHIFT	PAGE_SHIFT

#define PMD_SHIFT	(PAGE_SHIFT + PTE_INDEX_SIZE)
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))

#define PGDIR_SHIFT	(PMD_SHIFT + PMD_INDEX_SIZE)
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

#define PMD_MASKED_BITS		0x1ff
#define PUD_MASKED_BITS		0x1ff

#endif 
