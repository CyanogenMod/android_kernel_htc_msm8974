#ifndef _PARISC_TLBFLUSH_H
#define _PARISC_TLBFLUSH_H


#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/mmu_context.h>


extern spinlock_t pa_tlb_lock;

#define purge_tlb_start(flags)	spin_lock_irqsave(&pa_tlb_lock, flags)
#define purge_tlb_end(flags)	spin_unlock_irqrestore(&pa_tlb_lock, flags)

extern void flush_tlb_all(void);
extern void flush_tlb_all_local(void *);


static inline void flush_tlb_mm(struct mm_struct *mm)
{
	BUG_ON(mm == &init_mm); 

#if 1 || defined(CONFIG_SMP)
	flush_tlb_all();
#else
	if (mm) {
		if (mm->context != 0)
			free_sid(mm->context);
		mm->context = alloc_sid();
		if (mm == current->active_mm)
			load_context(mm->context);
	}
#endif
}

static inline void flush_tlb_page(struct vm_area_struct *vma,
	unsigned long addr)
{
	unsigned long flags;

	

	mb();
	mtsp(vma->vm_mm->context,1);
	purge_tlb_start(flags);
	pdtlb(addr);
	pitlb(addr);
	purge_tlb_end(flags);
}

void __flush_tlb_range(unsigned long sid,
	unsigned long start, unsigned long end);

#define flush_tlb_range(vma,start,end) __flush_tlb_range((vma)->vm_mm->context,start,end)

#define flush_tlb_kernel_range(start, end) __flush_tlb_range(0,start,end)

#endif
