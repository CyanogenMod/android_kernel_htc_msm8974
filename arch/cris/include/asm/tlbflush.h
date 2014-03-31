#ifndef _CRIS_TLBFLUSH_H
#define _CRIS_TLBFLUSH_H

#include <linux/mm.h>
#include <asm/processor.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>


extern void __flush_tlb_all(void);
extern void __flush_tlb_mm(struct mm_struct *mm);
extern void __flush_tlb_page(struct vm_area_struct *vma,
			   unsigned long addr);

#ifdef CONFIG_SMP
extern void flush_tlb_all(void);
extern void flush_tlb_mm(struct mm_struct *mm);
extern void flush_tlb_page(struct vm_area_struct *vma, 
			   unsigned long addr);
#else
#define flush_tlb_all __flush_tlb_all
#define flush_tlb_mm __flush_tlb_mm
#define flush_tlb_page __flush_tlb_page
#endif

static inline void flush_tlb_range(struct vm_area_struct * vma, unsigned long start, unsigned long end)
{
	flush_tlb_mm(vma->vm_mm);
}

static inline void flush_tlb(void)
{
	flush_tlb_mm(current->mm);
}

#define flush_tlb_kernel_range(start, end) flush_tlb_all()

#endif 
