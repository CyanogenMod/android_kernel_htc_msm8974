#ifndef __ASM_TLB_H
#define __ASM_TLB_H

#define tlb_start_vma(tlb, vma) 				\
	do {							\
		if (!tlb->fullmm)				\
			flush_cache_range(vma, vma->vm_start, vma->vm_end); \
	}  while (0)
#define tlb_end_vma(tlb, vma) do { } while (0)
#define __tlb_remove_tlb_entry(tlb, ptep, address) do { } while (0)

#define tlb_flush(tlb) flush_tlb_mm((tlb)->mm)

#include <asm-generic/tlb.h>

#endif 
