#ifndef __ASM_GENERIC_TLBFLUSH_H
#define __ASM_GENERIC_TLBFLUSH_H
#ifdef CONFIG_MMU
#error need to implement an architecture specific asm/tlbflush.h
#endif

#include <linux/bug.h>

static inline void flush_tlb_mm(struct mm_struct *mm)
{
	BUG();
}


#endif 
