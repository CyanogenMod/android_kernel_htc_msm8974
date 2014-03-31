#ifndef _S390_TLBFLUSH_H
#define _S390_TLBFLUSH_H

#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/processor.h>
#include <asm/pgalloc.h>

static inline void __tlb_flush_local(void)
{
	asm volatile("ptlb" : : : "memory");
}

#ifdef CONFIG_SMP
void smp_ptlb_all(void);

static inline void __tlb_flush_global(void)
{
	register unsigned long reg2 asm("2");
	register unsigned long reg3 asm("3");
	register unsigned long reg4 asm("4");
	long dummy;

#ifndef __s390x__
	if (!MACHINE_HAS_CSP) {
		smp_ptlb_all();
		return;
	}
#endif 

	dummy = 0;
	reg2 = reg3 = 0;
	reg4 = ((unsigned long) &dummy) + 1;
	asm volatile(
		"	csp	%0,%2"
		: : "d" (reg2), "d" (reg3), "d" (reg4), "m" (dummy) : "cc" );
}

static inline void __tlb_flush_full(struct mm_struct *mm)
{
	cpumask_t local_cpumask;

	preempt_disable();
	cpumask_copy(&local_cpumask, cpumask_of(smp_processor_id()));
	if (cpumask_equal(mm_cpumask(mm), &local_cpumask))
		__tlb_flush_local();
	else
		__tlb_flush_global();
	preempt_enable();
}
#else
#define __tlb_flush_full(mm)	__tlb_flush_local()
#define __tlb_flush_global()	__tlb_flush_local()
#endif

static inline void __tlb_flush_idte(unsigned long asce)
{
	asm volatile(
		"	.insn	rrf,0xb98e0000,0,%0,%1,0"
		: : "a" (2048), "a" (asce) : "cc" );
}

static inline void __tlb_flush_mm(struct mm_struct * mm)
{
	if (unlikely(cpumask_empty(mm_cpumask(mm))))
		return;
	if (MACHINE_HAS_IDTE && list_empty(&mm->context.gmap_list))
		__tlb_flush_idte((unsigned long) mm->pgd |
				 mm->context.asce_bits);
	else
		__tlb_flush_full(mm);
}

static inline void __tlb_flush_mm_cond(struct mm_struct * mm)
{
	spin_lock(&mm->page_table_lock);
	if (mm->context.flush_mm) {
		__tlb_flush_mm(mm);
		mm->context.flush_mm = 0;
	}
	spin_unlock(&mm->page_table_lock);
}


#define flush_tlb()				do { } while (0)
#define flush_tlb_all()				do { } while (0)
#define flush_tlb_page(vma, addr)		do { } while (0)

static inline void flush_tlb_mm(struct mm_struct *mm)
{
	__tlb_flush_mm_cond(mm);
}

static inline void flush_tlb_range(struct vm_area_struct *vma,
				   unsigned long start, unsigned long end)
{
	__tlb_flush_mm_cond(vma->vm_mm);
}

static inline void flush_tlb_kernel_range(unsigned long start,
					  unsigned long end)
{
	__tlb_flush_mm(&init_mm);
}

#endif 
