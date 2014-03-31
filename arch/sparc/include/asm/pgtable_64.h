/*
 * pgtable.h: SpitFire page table operations.
 *
 * Copyright 1996,1997 David S. Miller (davem@caip.rutgers.edu)
 * Copyright 1997,1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#ifndef _SPARC64_PGTABLE_H
#define _SPARC64_PGTABLE_H


#include <linux/compiler.h>
#include <linux/const.h>
#include <asm/types.h>
#include <asm/spitfire.h>
#include <asm/asi.h>
#include <asm/page.h>
#include <asm/processor.h>

#include <asm-generic/pgtable-nopud.h>

#define	TLBTEMP_BASE		_AC(0x0000000006000000,UL)
#define	TSBMAP_BASE		_AC(0x0000000008000000,UL)
#define MODULES_VADDR		_AC(0x0000000010000000,UL)
#define MODULES_LEN		_AC(0x00000000e0000000,UL)
#define MODULES_END		_AC(0x00000000f0000000,UL)
#define LOW_OBP_ADDRESS		_AC(0x00000000f0000000,UL)
#define HI_OBP_ADDRESS		_AC(0x0000000100000000,UL)
#define VMALLOC_START		_AC(0x0000000100000000,UL)
#define VMALLOC_END		_AC(0x0000010000000000,UL)
#define VMEMMAP_BASE		_AC(0x0000010000000000,UL)

#define vmemmap			((struct page *)VMEMMAP_BASE)


#define PMD_SHIFT	(PAGE_SHIFT + (PAGE_SHIFT-3))
#define PMD_SIZE	(_AC(1,UL) << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))
#define PMD_BITS	(PAGE_SHIFT - 2)

#define PGDIR_SHIFT	(PAGE_SHIFT + (PAGE_SHIFT-3) + PMD_BITS)
#define PGDIR_SIZE	(_AC(1,UL) << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))
#define PGDIR_BITS	(PAGE_SHIFT - 2)

#ifndef __ASSEMBLY__

#include <linux/sched.h>

#define PTRS_PER_PTE	(1UL << (PAGE_SHIFT-3))
#define PTRS_PER_PMD	(1UL << PMD_BITS)
#define PTRS_PER_PGD	(1UL << PGDIR_BITS)

#define FIRST_USER_ADDRESS	0

#define pte_ERROR(e)	__builtin_trap()
#define pmd_ERROR(e)	__builtin_trap()
#define pgd_ERROR(e)	__builtin_trap()

#endif 

#define _PAGE_VALID	  _AC(0x8000000000000000,UL) 
#define _PAGE_R	  	  _AC(0x8000000000000000,UL) 
#define _PAGE_SPECIAL     _AC(0x0200000000000000,UL) 

#define __HAVE_ARCH_PTE_SPECIAL

#define _PAGE_SZ4MB_4U	  _AC(0x6000000000000000,UL) 
#define _PAGE_SZ512K_4U	  _AC(0x4000000000000000,UL) 
#define _PAGE_SZ64K_4U	  _AC(0x2000000000000000,UL) 
#define _PAGE_SZ8K_4U	  _AC(0x0000000000000000,UL) 
#define _PAGE_NFO_4U	  _AC(0x1000000000000000,UL) 
#define _PAGE_IE_4U	  _AC(0x0800000000000000,UL) 
#define _PAGE_SOFT2_4U	  _AC(0x07FC000000000000,UL) 
#define _PAGE_SPECIAL_4U  _AC(0x0200000000000000,UL) 
#define _PAGE_RES1_4U	  _AC(0x0002000000000000,UL) 
#define _PAGE_SZ32MB_4U	  _AC(0x0001000000000000,UL) 
#define _PAGE_SZ256MB_4U  _AC(0x2001000000000000,UL) 
#define _PAGE_SZALL_4U	  _AC(0x6001000000000000,UL) 
#define _PAGE_SN_4U	  _AC(0x0000800000000000,UL) 
#define _PAGE_RES2_4U	  _AC(0x0000780000000000,UL) 
#define _PAGE_PADDR_4U	  _AC(0x000007FFFFFFE000,UL) 
#define _PAGE_SOFT_4U	  _AC(0x0000000000001F80,UL) 
#define _PAGE_EXEC_4U	  _AC(0x0000000000001000,UL) 
#define _PAGE_MODIFIED_4U _AC(0x0000000000000800,UL) 
#define _PAGE_FILE_4U	  _AC(0x0000000000000800,UL) 
#define _PAGE_ACCESSED_4U _AC(0x0000000000000400,UL) 
#define _PAGE_READ_4U	  _AC(0x0000000000000200,UL) 
#define _PAGE_WRITE_4U	  _AC(0x0000000000000100,UL) 
#define _PAGE_PRESENT_4U  _AC(0x0000000000000080,UL) 
#define _PAGE_L_4U	  _AC(0x0000000000000040,UL) 
#define _PAGE_CP_4U	  _AC(0x0000000000000020,UL) 
#define _PAGE_CV_4U	  _AC(0x0000000000000010,UL) 
#define _PAGE_E_4U	  _AC(0x0000000000000008,UL) 
#define _PAGE_P_4U	  _AC(0x0000000000000004,UL) 
#define _PAGE_W_4U	  _AC(0x0000000000000002,UL) 

#define _PAGE_NFO_4V	  _AC(0x4000000000000000,UL) 
#define _PAGE_SOFT2_4V	  _AC(0x3F00000000000000,UL) 
#define _PAGE_MODIFIED_4V _AC(0x2000000000000000,UL) 
#define _PAGE_ACCESSED_4V _AC(0x1000000000000000,UL) 
#define _PAGE_READ_4V	  _AC(0x0800000000000000,UL) 
#define _PAGE_WRITE_4V	  _AC(0x0400000000000000,UL) 
#define _PAGE_SPECIAL_4V  _AC(0x0200000000000000,UL) 
#define _PAGE_PADDR_4V	  _AC(0x00FFFFFFFFFFE000,UL) 
#define _PAGE_IE_4V	  _AC(0x0000000000001000,UL) 
#define _PAGE_E_4V	  _AC(0x0000000000000800,UL) 
#define _PAGE_CP_4V	  _AC(0x0000000000000400,UL) 
#define _PAGE_CV_4V	  _AC(0x0000000000000200,UL) 
#define _PAGE_P_4V	  _AC(0x0000000000000100,UL) 
#define _PAGE_EXEC_4V	  _AC(0x0000000000000080,UL) 
#define _PAGE_W_4V	  _AC(0x0000000000000040,UL) 
#define _PAGE_SOFT_4V	  _AC(0x0000000000000030,UL) 
#define _PAGE_FILE_4V	  _AC(0x0000000000000020,UL) 
#define _PAGE_PRESENT_4V  _AC(0x0000000000000010,UL) 
#define _PAGE_RESV_4V	  _AC(0x0000000000000008,UL) 
#define _PAGE_SZ16GB_4V	  _AC(0x0000000000000007,UL) 
#define _PAGE_SZ2GB_4V	  _AC(0x0000000000000006,UL) 
#define _PAGE_SZ256MB_4V  _AC(0x0000000000000005,UL) 
#define _PAGE_SZ32MB_4V	  _AC(0x0000000000000004,UL) 
#define _PAGE_SZ4MB_4V	  _AC(0x0000000000000003,UL) 
#define _PAGE_SZ512K_4V	  _AC(0x0000000000000002,UL) 
#define _PAGE_SZ64K_4V	  _AC(0x0000000000000001,UL) 
#define _PAGE_SZ8K_4V	  _AC(0x0000000000000000,UL) 
#define _PAGE_SZALL_4V	  _AC(0x0000000000000007,UL) 

#if PAGE_SHIFT == 13
#define _PAGE_SZBITS_4U	_PAGE_SZ8K_4U
#define _PAGE_SZBITS_4V	_PAGE_SZ8K_4V
#elif PAGE_SHIFT == 16
#define _PAGE_SZBITS_4U	_PAGE_SZ64K_4U
#define _PAGE_SZBITS_4V	_PAGE_SZ64K_4V
#else
#error Wrong PAGE_SHIFT specified
#endif

#if defined(CONFIG_HUGETLB_PAGE_SIZE_4MB)
#define _PAGE_SZHUGE_4U	_PAGE_SZ4MB_4U
#define _PAGE_SZHUGE_4V	_PAGE_SZ4MB_4V
#elif defined(CONFIG_HUGETLB_PAGE_SIZE_512K)
#define _PAGE_SZHUGE_4U	_PAGE_SZ512K_4U
#define _PAGE_SZHUGE_4V	_PAGE_SZ512K_4V
#elif defined(CONFIG_HUGETLB_PAGE_SIZE_64K)
#define _PAGE_SZHUGE_4U	_PAGE_SZ64K_4U
#define _PAGE_SZHUGE_4V	_PAGE_SZ64K_4V
#endif

#define __P000	__pgprot(0)
#define __P001	__pgprot(0)
#define __P010	__pgprot(0)
#define __P011	__pgprot(0)
#define __P100	__pgprot(0)
#define __P101	__pgprot(0)
#define __P110	__pgprot(0)
#define __P111	__pgprot(0)

#define __S000	__pgprot(0)
#define __S001	__pgprot(0)
#define __S010	__pgprot(0)
#define __S011	__pgprot(0)
#define __S100	__pgprot(0)
#define __S101	__pgprot(0)
#define __S110	__pgprot(0)
#define __S111	__pgprot(0)

#ifndef __ASSEMBLY__

extern pte_t mk_pte_io(unsigned long, pgprot_t, int, unsigned long);

extern unsigned long pte_sz_bits(unsigned long size);

extern pgprot_t PAGE_KERNEL;
extern pgprot_t PAGE_KERNEL_LOCKED;
extern pgprot_t PAGE_COPY;
extern pgprot_t PAGE_SHARED;

extern unsigned long _PAGE_IE;
extern unsigned long _PAGE_E;
extern unsigned long _PAGE_CACHE;

extern unsigned long pg_iobits;
extern unsigned long _PAGE_ALL_SZ_BITS;
extern unsigned long _PAGE_SZBITS;

extern struct page *mem_map_zero;
#define ZERO_PAGE(vaddr)	(mem_map_zero)

static inline pte_t pfn_pte(unsigned long pfn, pgprot_t prot)
{
	unsigned long paddr = pfn << PAGE_SHIFT;
	unsigned long sz_bits;

	sz_bits = 0UL;
	if (_PAGE_SZBITS_4U != 0UL || _PAGE_SZBITS_4V != 0UL) {
		__asm__ __volatile__(
		"\n661:	sethi		%%uhi(%1), %0\n"
		"	sllx		%0, 32, %0\n"
		"	.section	.sun4v_2insn_patch, \"ax\"\n"
		"	.word		661b\n"
		"	mov		%2, %0\n"
		"	nop\n"
		"	.previous\n"
		: "=r" (sz_bits)
		: "i" (_PAGE_SZBITS_4U), "i" (_PAGE_SZBITS_4V));
	}
	return __pte(paddr | sz_bits | pgprot_val(prot));
}
#define mk_pte(page, pgprot)	pfn_pte(page_to_pfn(page), (pgprot))

static inline unsigned long pte_pfn(pte_t pte)
{
	unsigned long ret;

	__asm__ __volatile__(
	"\n661:	sllx		%1, %2, %0\n"
	"	srlx		%0, %3, %0\n"
	"	.section	.sun4v_2insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	sllx		%1, %4, %0\n"
	"	srlx		%0, %5, %0\n"
	"	.previous\n"
	: "=r" (ret)
	: "r" (pte_val(pte)),
	  "i" (21), "i" (21 + PAGE_SHIFT),
	  "i" (8), "i" (8 + PAGE_SHIFT));

	return ret;
}
#define pte_page(x) pfn_to_page(pte_pfn(x))

static inline pte_t pte_modify(pte_t pte, pgprot_t prot)
{
	unsigned long mask, tmp;


	__asm__ __volatile__(
	"\n661:	sethi		%%uhi(%2), %1\n"
	"	sethi		%%hi(%2), %0\n"
	"\n662:	or		%1, %%ulo(%2), %1\n"
	"	or		%0, %%lo(%2), %0\n"
	"\n663:	sllx		%1, 32, %1\n"
	"	or		%0, %1, %0\n"
	"	.section	.sun4v_2insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	sethi		%%uhi(%3), %1\n"
	"	sethi		%%hi(%3), %0\n"
	"	.word		662b\n"
	"	or		%1, %%ulo(%3), %1\n"
	"	or		%0, %%lo(%3), %0\n"
	"	.word		663b\n"
	"	sllx		%1, 32, %1\n"
	"	or		%0, %1, %0\n"
	"	.previous\n"
	: "=r" (mask), "=r" (tmp)
	: "i" (_PAGE_PADDR_4U | _PAGE_MODIFIED_4U | _PAGE_ACCESSED_4U |
	       _PAGE_CP_4U | _PAGE_CV_4U | _PAGE_E_4U | _PAGE_PRESENT_4U |
	       _PAGE_SZBITS_4U | _PAGE_SPECIAL),
	  "i" (_PAGE_PADDR_4V | _PAGE_MODIFIED_4V | _PAGE_ACCESSED_4V |
	       _PAGE_CP_4V | _PAGE_CV_4V | _PAGE_E_4V | _PAGE_PRESENT_4V |
	       _PAGE_SZBITS_4V | _PAGE_SPECIAL));

	return __pte((pte_val(pte) & mask) | (pgprot_val(prot) & ~mask));
}

static inline pte_t pgoff_to_pte(unsigned long off)
{
	off <<= PAGE_SHIFT;

	__asm__ __volatile__(
	"\n661:	or		%0, %2, %0\n"
	"	.section	.sun4v_1insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	or		%0, %3, %0\n"
	"	.previous\n"
	: "=r" (off)
	: "0" (off), "i" (_PAGE_FILE_4U), "i" (_PAGE_FILE_4V));

	return __pte(off);
}

static inline pgprot_t pgprot_noncached(pgprot_t prot)
{
	unsigned long val = pgprot_val(prot);

	__asm__ __volatile__(
	"\n661:	andn		%0, %2, %0\n"
	"	or		%0, %3, %0\n"
	"	.section	.sun4v_2insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	andn		%0, %4, %0\n"
	"	or		%0, %5, %0\n"
	"	.previous\n"
	: "=r" (val)
	: "0" (val), "i" (_PAGE_CP_4U | _PAGE_CV_4U), "i" (_PAGE_E_4U),
	             "i" (_PAGE_CP_4V | _PAGE_CV_4V), "i" (_PAGE_E_4V));

	return __pgprot(val);
}
#define pgprot_noncached pgprot_noncached

#ifdef CONFIG_HUGETLB_PAGE
static inline pte_t pte_mkhuge(pte_t pte)
{
	unsigned long mask;

	__asm__ __volatile__(
	"\n661:	sethi		%%uhi(%1), %0\n"
	"	sllx		%0, 32, %0\n"
	"	.section	.sun4v_2insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	mov		%2, %0\n"
	"	nop\n"
	"	.previous\n"
	: "=r" (mask)
	: "i" (_PAGE_SZHUGE_4U), "i" (_PAGE_SZHUGE_4V));

	return __pte(pte_val(pte) | mask);
}
#endif

static inline pte_t pte_mkdirty(pte_t pte)
{
	unsigned long val = pte_val(pte), tmp;

	__asm__ __volatile__(
	"\n661:	or		%0, %3, %0\n"
	"	nop\n"
	"\n662:	nop\n"
	"	nop\n"
	"	.section	.sun4v_2insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	sethi		%%uhi(%4), %1\n"
	"	sllx		%1, 32, %1\n"
	"	.word		662b\n"
	"	or		%1, %%lo(%4), %1\n"
	"	or		%0, %1, %0\n"
	"	.previous\n"
	: "=r" (val), "=r" (tmp)
	: "0" (val), "i" (_PAGE_MODIFIED_4U | _PAGE_W_4U),
	  "i" (_PAGE_MODIFIED_4V | _PAGE_W_4V));

	return __pte(val);
}

static inline pte_t pte_mkclean(pte_t pte)
{
	unsigned long val = pte_val(pte), tmp;

	__asm__ __volatile__(
	"\n661:	andn		%0, %3, %0\n"
	"	nop\n"
	"\n662:	nop\n"
	"	nop\n"
	"	.section	.sun4v_2insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	sethi		%%uhi(%4), %1\n"
	"	sllx		%1, 32, %1\n"
	"	.word		662b\n"
	"	or		%1, %%lo(%4), %1\n"
	"	andn		%0, %1, %0\n"
	"	.previous\n"
	: "=r" (val), "=r" (tmp)
	: "0" (val), "i" (_PAGE_MODIFIED_4U | _PAGE_W_4U),
	  "i" (_PAGE_MODIFIED_4V | _PAGE_W_4V));

	return __pte(val);
}

static inline pte_t pte_mkwrite(pte_t pte)
{
	unsigned long val = pte_val(pte), mask;

	__asm__ __volatile__(
	"\n661:	mov		%1, %0\n"
	"	nop\n"
	"	.section	.sun4v_2insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	sethi		%%uhi(%2), %0\n"
	"	sllx		%0, 32, %0\n"
	"	.previous\n"
	: "=r" (mask)
	: "i" (_PAGE_WRITE_4U), "i" (_PAGE_WRITE_4V));

	return __pte(val | mask);
}

static inline pte_t pte_wrprotect(pte_t pte)
{
	unsigned long val = pte_val(pte), tmp;

	__asm__ __volatile__(
	"\n661:	andn		%0, %3, %0\n"
	"	nop\n"
	"\n662:	nop\n"
	"	nop\n"
	"	.section	.sun4v_2insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	sethi		%%uhi(%4), %1\n"
	"	sllx		%1, 32, %1\n"
	"	.word		662b\n"
	"	or		%1, %%lo(%4), %1\n"
	"	andn		%0, %1, %0\n"
	"	.previous\n"
	: "=r" (val), "=r" (tmp)
	: "0" (val), "i" (_PAGE_WRITE_4U | _PAGE_W_4U),
	  "i" (_PAGE_WRITE_4V | _PAGE_W_4V));

	return __pte(val);
}

static inline pte_t pte_mkold(pte_t pte)
{
	unsigned long mask;

	__asm__ __volatile__(
	"\n661:	mov		%1, %0\n"
	"	nop\n"
	"	.section	.sun4v_2insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	sethi		%%uhi(%2), %0\n"
	"	sllx		%0, 32, %0\n"
	"	.previous\n"
	: "=r" (mask)
	: "i" (_PAGE_ACCESSED_4U), "i" (_PAGE_ACCESSED_4V));

	mask |= _PAGE_R;

	return __pte(pte_val(pte) & ~mask);
}

static inline pte_t pte_mkyoung(pte_t pte)
{
	unsigned long mask;

	__asm__ __volatile__(
	"\n661:	mov		%1, %0\n"
	"	nop\n"
	"	.section	.sun4v_2insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	sethi		%%uhi(%2), %0\n"
	"	sllx		%0, 32, %0\n"
	"	.previous\n"
	: "=r" (mask)
	: "i" (_PAGE_ACCESSED_4U), "i" (_PAGE_ACCESSED_4V));

	mask |= _PAGE_R;

	return __pte(pte_val(pte) | mask);
}

static inline pte_t pte_mkspecial(pte_t pte)
{
	pte_val(pte) |= _PAGE_SPECIAL;
	return pte;
}

static inline unsigned long pte_young(pte_t pte)
{
	unsigned long mask;

	__asm__ __volatile__(
	"\n661:	mov		%1, %0\n"
	"	nop\n"
	"	.section	.sun4v_2insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	sethi		%%uhi(%2), %0\n"
	"	sllx		%0, 32, %0\n"
	"	.previous\n"
	: "=r" (mask)
	: "i" (_PAGE_ACCESSED_4U), "i" (_PAGE_ACCESSED_4V));

	return (pte_val(pte) & mask);
}

static inline unsigned long pte_dirty(pte_t pte)
{
	unsigned long mask;

	__asm__ __volatile__(
	"\n661:	mov		%1, %0\n"
	"	nop\n"
	"	.section	.sun4v_2insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	sethi		%%uhi(%2), %0\n"
	"	sllx		%0, 32, %0\n"
	"	.previous\n"
	: "=r" (mask)
	: "i" (_PAGE_MODIFIED_4U), "i" (_PAGE_MODIFIED_4V));

	return (pte_val(pte) & mask);
}

static inline unsigned long pte_write(pte_t pte)
{
	unsigned long mask;

	__asm__ __volatile__(
	"\n661:	mov		%1, %0\n"
	"	nop\n"
	"	.section	.sun4v_2insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	sethi		%%uhi(%2), %0\n"
	"	sllx		%0, 32, %0\n"
	"	.previous\n"
	: "=r" (mask)
	: "i" (_PAGE_WRITE_4U), "i" (_PAGE_WRITE_4V));

	return (pte_val(pte) & mask);
}

static inline unsigned long pte_exec(pte_t pte)
{
	unsigned long mask;

	__asm__ __volatile__(
	"\n661:	sethi		%%hi(%1), %0\n"
	"	.section	.sun4v_1insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	mov		%2, %0\n"
	"	.previous\n"
	: "=r" (mask)
	: "i" (_PAGE_EXEC_4U), "i" (_PAGE_EXEC_4V));

	return (pte_val(pte) & mask);
}

static inline unsigned long pte_file(pte_t pte)
{
	unsigned long val = pte_val(pte);

	__asm__ __volatile__(
	"\n661:	and		%0, %2, %0\n"
	"	.section	.sun4v_1insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	and		%0, %3, %0\n"
	"	.previous\n"
	: "=r" (val)
	: "0" (val), "i" (_PAGE_FILE_4U), "i" (_PAGE_FILE_4V));

	return val;
}

static inline unsigned long pte_present(pte_t pte)
{
	unsigned long val = pte_val(pte);

	__asm__ __volatile__(
	"\n661:	and		%0, %2, %0\n"
	"	.section	.sun4v_1insn_patch, \"ax\"\n"
	"	.word		661b\n"
	"	and		%0, %3, %0\n"
	"	.previous\n"
	: "=r" (val)
	: "0" (val), "i" (_PAGE_PRESENT_4U), "i" (_PAGE_PRESENT_4V));

	return val;
}

static inline unsigned long pte_special(pte_t pte)
{
	return pte_val(pte) & _PAGE_SPECIAL;
}

#define pmd_set(pmdp, ptep)	\
	(pmd_val(*(pmdp)) = (__pa((unsigned long) (ptep)) >> 11UL))
#define pud_set(pudp, pmdp)	\
	(pud_val(*(pudp)) = (__pa((unsigned long) (pmdp)) >> 11UL))
#define __pmd_page(pmd)		\
	((unsigned long) __va((((unsigned long)pmd_val(pmd))<<11UL)))
#define pmd_page(pmd) 			virt_to_page((void *)__pmd_page(pmd))
#define pud_page_vaddr(pud)		\
	((unsigned long) __va((((unsigned long)pud_val(pud))<<11UL)))
#define pud_page(pud) 			virt_to_page((void *)pud_page_vaddr(pud))
#define pmd_none(pmd)			(!pmd_val(pmd))
#define pmd_bad(pmd)			(0)
#define pmd_present(pmd)		(pmd_val(pmd) != 0U)
#define pmd_clear(pmdp)			(pmd_val(*(pmdp)) = 0U)
#define pud_none(pud)			(!pud_val(pud))
#define pud_bad(pud)			(0)
#define pud_present(pud)		(pud_val(pud) != 0U)
#define pud_clear(pudp)			(pud_val(*(pudp)) = 0U)

#define pte_none(pte) 			(!pte_val(pte))

#define pgd_index(address)	(((address) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))
#define pgd_offset(mm, address)	((mm)->pgd + pgd_index(address))

#define pgd_offset_k(address) pgd_offset(&init_mm, address)

#define pmd_offset(pudp, address)	\
	((pmd_t *) pud_page_vaddr(*(pudp)) + \
	 (((address) >> PMD_SHIFT) & (PTRS_PER_PMD-1)))

#define pte_index(dir, address)	\
	((pte_t *) __pmd_page(*(dir)) + \
	 ((address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1)))
#define pte_offset_kernel		pte_index
#define pte_offset_map			pte_index
#define pte_unmap(pte)			do { } while (0)

extern void tlb_batch_add(struct mm_struct *mm, unsigned long vaddr,
			  pte_t *ptep, pte_t orig, int fullmm);

static inline void __set_pte_at(struct mm_struct *mm, unsigned long addr,
			     pte_t *ptep, pte_t pte, int fullmm)
{
	pte_t orig = *ptep;

	*ptep = pte;

	if (likely(mm != &init_mm) && (pte_val(orig) & _PAGE_VALID))
		tlb_batch_add(mm, addr, ptep, orig, fullmm);
}

#define set_pte_at(mm,addr,ptep,pte)	\
	__set_pte_at((mm), (addr), (ptep), (pte), 0)

#define pte_clear(mm,addr,ptep)		\
	set_pte_at((mm), (addr), (ptep), __pte(0UL))

#define __HAVE_ARCH_PTE_CLEAR_NOT_PRESENT_FULL
#define pte_clear_not_present_full(mm,addr,ptep,fullmm)	\
	__set_pte_at((mm), (addr), (ptep), __pte(0UL), (fullmm))

#ifdef DCACHE_ALIASING_POSSIBLE
#define __HAVE_ARCH_MOVE_PTE
#define move_pte(pte, prot, old_addr, new_addr)				\
({									\
	pte_t newpte = (pte);						\
	if (tlb_type != hypervisor && pte_present(pte)) {		\
		unsigned long this_pfn = pte_pfn(pte);			\
									\
		if (pfn_valid(this_pfn) &&				\
		    (((old_addr) ^ (new_addr)) & (1 << 13)))		\
			flush_dcache_page_all(current->mm,		\
					      pfn_to_page(this_pfn));	\
	}								\
	newpte;								\
})
#endif

extern pgd_t swapper_pg_dir[2048];
extern pmd_t swapper_low_pmd_dir[2048];

extern void paging_init(void);
extern unsigned long find_ecache_flush_span(unsigned long size);

struct seq_file;
extern void mmu_info(struct seq_file *);

#define mmu_lockarea(vaddr, len)		(vaddr)
#define mmu_unlockarea(vaddr, len)		do { } while(0)

struct vm_area_struct;
extern void update_mmu_cache(struct vm_area_struct *, unsigned long, pte_t *);

#define __swp_type(entry)	(((entry).val >> PAGE_SHIFT) & 0xffUL)
#define __swp_offset(entry)	((entry).val >> (PAGE_SHIFT + 8UL))
#define __swp_entry(type, offset)	\
	( (swp_entry_t) \
	  { \
		(((long)(type) << PAGE_SHIFT) | \
                 ((long)(offset) << (PAGE_SHIFT + 8UL))) \
	  } )
#define __pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)		((pte_t) { (x).val })

extern unsigned long pte_file(pte_t);
#define pte_to_pgoff(pte)	(pte_val(pte) >> PAGE_SHIFT)
extern pte_t pgoff_to_pte(unsigned long);
#define PTE_FILE_MAX_BITS	(64UL - PAGE_SHIFT - 1UL)

extern unsigned long sparc64_valid_addr_bitmap[];

static inline bool kern_addr_valid(unsigned long addr)
{
	unsigned long paddr = __pa(addr);

	if ((paddr >> 41UL) != 0UL)
		return false;
	return test_bit(paddr >> 22, sparc64_valid_addr_bitmap);
}

extern int page_in_phys_avail(unsigned long paddr);

#define MK_IOSPACE_PFN(space, pfn)	(pfn | (space << (BITS_PER_LONG - 4)))
#define GET_IOSPACE(pfn)		(pfn >> (BITS_PER_LONG - 4))
#define GET_PFN(pfn)			(pfn & 0x0fffffffffffffffUL)

extern int remap_pfn_range(struct vm_area_struct *, unsigned long, unsigned long,
			   unsigned long, pgprot_t);

static inline int io_remap_pfn_range(struct vm_area_struct *vma,
				     unsigned long from, unsigned long pfn,
				     unsigned long size, pgprot_t prot)
{
	unsigned long offset = GET_PFN(pfn) << PAGE_SHIFT;
	int space = GET_IOSPACE(pfn);
	unsigned long phys_base;

	phys_base = offset | (((unsigned long) space) << 32UL);

	return remap_pfn_range(vma, from, phys_base >> PAGE_SHIFT, size, prot);
}

#include <asm-generic/pgtable.h>

#define HAVE_ARCH_UNMAPPED_AREA
#define HAVE_ARCH_UNMAPPED_AREA_TOPDOWN

extern unsigned long get_fb_unmapped_area(struct file *filp, unsigned long,
					  unsigned long, unsigned long,
					  unsigned long);
#define HAVE_ARCH_FB_UNMAPPED_AREA

extern void pgtable_cache_init(void);
extern void sun4v_register_fault_status(void);
extern void sun4v_ktsb_register(void);
extern void __init cheetah_ecache_flush_init(void);
extern void sun4v_patch_tlb_handlers(void);

extern unsigned long cmdline_memory_size;

extern asmlinkage void do_sparc64_fault(struct pt_regs *regs);

#endif 

#endif 
