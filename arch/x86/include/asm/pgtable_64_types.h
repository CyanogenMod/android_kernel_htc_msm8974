#ifndef _ASM_X86_PGTABLE_64_DEFS_H
#define _ASM_X86_PGTABLE_64_DEFS_H

#ifndef __ASSEMBLY__
#include <linux/types.h>

typedef unsigned long	pteval_t;
typedef unsigned long	pmdval_t;
typedef unsigned long	pudval_t;
typedef unsigned long	pgdval_t;
typedef unsigned long	pgprotval_t;

typedef struct { pteval_t pte; } pte_t;

#endif	

#define SHARED_KERNEL_PMD	0
#define PAGETABLE_LEVELS	4

#define PGDIR_SHIFT	39
#define PTRS_PER_PGD	512

#define PUD_SHIFT	30
#define PTRS_PER_PUD	512

#define PMD_SHIFT	21
#define PTRS_PER_PMD	512

#define PTRS_PER_PTE	512

#define PMD_SIZE	(_AC(1, UL) << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE - 1))
#define PUD_SIZE	(_AC(1, UL) << PUD_SHIFT)
#define PUD_MASK	(~(PUD_SIZE - 1))
#define PGDIR_SIZE	(_AC(1, UL) << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE - 1))

#define MAXMEM		 _AC(__AC(1, UL) << MAX_PHYSMEM_BITS, UL)
#define VMALLOC_START    _AC(0xffffc90000000000, UL)
#define VMALLOC_END      _AC(0xffffe8ffffffffff, UL)
#define VMEMMAP_START	 _AC(0xffffea0000000000, UL)
#define MODULES_VADDR    _AC(0xffffffffa0000000, UL)
#define MODULES_END      _AC(0xffffffffff000000, UL)
#define MODULES_LEN   (MODULES_END - MODULES_VADDR)

#endif 
