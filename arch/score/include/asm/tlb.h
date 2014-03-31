#ifndef _ASM_SCORE_TLB_H
#define _ASM_SCORE_TLB_H

#define tlb_start_vma(tlb, vma)		do {} while (0)
#define tlb_end_vma(tlb, vma)		do {} while (0)
#define __tlb_remove_tlb_entry(tlb, ptep, address) do {} while (0)
#define tlb_flush(tlb)			flush_tlb_mm((tlb)->mm)

extern void score7_FTLB_refill_Handler(void);

#include <asm-generic/tlb.h>

#endif 
