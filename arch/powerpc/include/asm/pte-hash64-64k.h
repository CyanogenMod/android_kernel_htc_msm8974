
#define _PAGE_SPECIAL	0x00000400 
#define _PAGE_HPTE_SUB	0x0ffff000 
#define _PAGE_HPTE_SUB0	0x08000000 
#define _PAGE_COMBO	0x10000000 
#define _PAGE_4K_PFN	0x20000000 

#define _PAGE_HASHPTE	_PAGE_HPTE_SUB

#define _PAGE_F_SECOND  0x00008000 
#define _PAGE_F_GIX     0x00007000 

#define _PAGE_HPTEFLAGS (_PAGE_BUSY | _PAGE_HASHPTE | _PAGE_COMBO)

#define PTE_RPN_SHIFT	(30)

#ifndef __ASSEMBLY__

#define __real_pte(e,p) 	((real_pte_t) { \
			(e), ((e) & _PAGE_COMBO) ? \
				(pte_val(*((p) + PTRS_PER_PTE))) : 0 })
#define __rpte_to_hidx(r,index)	((pte_val((r).pte) & _PAGE_COMBO) ? \
        (((r).hidx >> ((index)<<2)) & 0xf) : ((pte_val((r).pte) >> 12) & 0xf))
#define __rpte_to_pte(r)	((r).pte)
#define __rpte_sub_valid(rpte, index) \
	(pte_val(rpte.pte) & (_PAGE_HPTE_SUB0 >> (index)))

#define pte_iterate_hashed_subpages(rpte, psize, va, index, shift)	    \
        do {                                                                \
                unsigned long __end = va + PAGE_SIZE;                       \
                unsigned __split = (psize == MMU_PAGE_4K ||                 \
				    psize == MMU_PAGE_64K_AP);              \
                shift = mmu_psize_defs[psize].shift;                        \
		for (index = 0; va < __end; index++, va += (1L << shift)) { \
		        if (!__split || __rpte_sub_valid(rpte, index)) do { \

#define pte_iterate_hashed_end() } while(0); } } while(0)

#define pte_pagesize_index(mm, addr, pte)	\
	(((pte) & _PAGE_COMBO)? MMU_PAGE_4K: MMU_PAGE_64K)

#define remap_4k_pfn(vma, addr, pfn, prot)				\
	remap_pfn_range((vma), (addr), (pfn), PAGE_SIZE,		\
			__pgprot(pgprot_val((prot)) | _PAGE_4K_PFN))

#endif	
