/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_AVR32_PGTABLE_2LEVEL_H
#define __ASM_AVR32_PGTABLE_2LEVEL_H

#include <asm-generic/pgtable-nopmd.h>

#define PGDIR_SHIFT	22
#define PTRS_PER_PGD	1024

#define PTRS_PER_PTE	1024

#ifndef __ASSEMBLY__
#define pte_ERROR(e) \
	printk("%s:%d: bad pte %08lx.\n", __FILE__, __LINE__, pte_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, pgd_val(e))

#define set_pte(pteptr, pteval) (*(pteptr) = pteval)
#define set_pte_at(mm,addr,ptep,pteval) set_pte(ptep, pteval)

#define set_pmd(pmdptr, pmdval) (*(pmdptr) = pmdval)

#define pte_pfn(x)		((unsigned long)(((x).pte >> PAGE_SHIFT)))
#define pfn_pte(pfn, prot)	__pte(((pfn) << PAGE_SHIFT) | pgprot_val(prot))
#define pfn_pmd(pfn, prot)	__pmd(((pfn) << PAGE_SHIFT) | pgprot_val(prot))

#endif 

#endif 
