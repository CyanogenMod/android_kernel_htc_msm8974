#ifndef __ASM_TLBFLUSH_H
#define __ASM_TLBFLUSH_H

#include <linux/mm.h>

extern void local_flush_tlb_all(void);
extern void local_flush_tlb_mm(struct mm_struct *mm);
extern void local_flush_tlb_range(struct vm_area_struct *vma,
	unsigned long start, unsigned long end);
extern void local_flush_tlb_kernel_range(unsigned long start,
	unsigned long end);
extern void local_flush_tlb_page(struct vm_area_struct *vma,
	unsigned long page);
extern void local_flush_tlb_one(unsigned long vaddr);

#ifdef CONFIG_SMP

extern void flush_tlb_all(void);
extern void flush_tlb_mm(struct mm_struct *);
extern void flush_tlb_range(struct vm_area_struct *vma, unsigned long,
	unsigned long);
extern void flush_tlb_kernel_range(unsigned long, unsigned long);
extern void flush_tlb_page(struct vm_area_struct *, unsigned long);
extern void flush_tlb_one(unsigned long vaddr);

#else 

#define flush_tlb_all()			local_flush_tlb_all()
#define flush_tlb_mm(mm)		local_flush_tlb_mm(mm)
#define flush_tlb_range(vma, vmaddr, end)	local_flush_tlb_range(vma, vmaddr, end)
#define flush_tlb_kernel_range(vmaddr,end) \
	local_flush_tlb_kernel_range(vmaddr, end)
#define flush_tlb_page(vma, page)	local_flush_tlb_page(vma, page)
#define flush_tlb_one(vaddr)		local_flush_tlb_one(vaddr)

#endif 

#endif 
