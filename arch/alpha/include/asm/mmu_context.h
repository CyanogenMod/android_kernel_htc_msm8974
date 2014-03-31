#ifndef __ALPHA_MMU_CONTEXT_H
#define __ALPHA_MMU_CONTEXT_H

/*
 * get a new mmu context..
 *
 * Copyright (C) 1996, Linus Torvalds
 */

#include <asm/machvec.h>
#include <asm/compiler.h>
#include <asm-generic/mm_hooks.h>


#ifndef __EXTERN_INLINE
#include <asm/io.h>
#endif


static inline unsigned long
__reload_thread(struct pcb_struct *pcb)
{
	register unsigned long a0 __asm__("$16");
	register unsigned long v0 __asm__("$0");

	a0 = virt_to_phys(pcb);
	__asm__ __volatile__(
		"call_pal %2 #__reload_thread"
		: "=r"(v0), "=r"(a0)
		: "i"(PAL_swpctx), "r"(a0)
		: "$1", "$22", "$23", "$24", "$25");

	return v0;
}


#define EV4_MAX_ASN 63
#define EV5_MAX_ASN 127
#define EV6_MAX_ASN 255

#ifdef CONFIG_ALPHA_GENERIC
# define MAX_ASN	(alpha_mv.max_asn)
#else
# ifdef CONFIG_ALPHA_EV4
#  define MAX_ASN	EV4_MAX_ASN
# elif defined(CONFIG_ALPHA_EV5)
#  define MAX_ASN	EV5_MAX_ASN
# else
#  define MAX_ASN	EV6_MAX_ASN
# endif
#endif


#include <asm/smp.h>
#ifdef CONFIG_SMP
#define cpu_last_asn(cpuid)	(cpu_data[cpuid].last_asn)
#else
extern unsigned long last_asn;
#define cpu_last_asn(cpuid)	last_asn
#endif 

#define WIDTH_HARDWARE_ASN	8
#define ASN_FIRST_VERSION (1UL << WIDTH_HARDWARE_ASN)
#define HARDWARE_ASN_MASK ((1UL << WIDTH_HARDWARE_ASN) - 1)


#ifndef __EXTERN_INLINE
#define __EXTERN_INLINE extern inline
#define __MMU_EXTERN_INLINE
#endif

extern inline unsigned long
__get_new_mm_context(struct mm_struct *mm, long cpu)
{
	unsigned long asn = cpu_last_asn(cpu);
	unsigned long next = asn + 1;

	if ((asn & HARDWARE_ASN_MASK) >= MAX_ASN) {
		tbiap();
		imb();
		next = (asn & ~HARDWARE_ASN_MASK) + ASN_FIRST_VERSION;
	}
	cpu_last_asn(cpu) = next;
	return next;
}

__EXTERN_INLINE void
ev5_switch_mm(struct mm_struct *prev_mm, struct mm_struct *next_mm,
	      struct task_struct *next)
{
	
	unsigned long asn;
	unsigned long mmc;
	long cpu = smp_processor_id();

#ifdef CONFIG_SMP
	cpu_data[cpu].asn_lock = 1;
	barrier();
#endif
	asn = cpu_last_asn(cpu);
	mmc = next_mm->context[cpu];
	if ((mmc ^ asn) & ~HARDWARE_ASN_MASK) {
		mmc = __get_new_mm_context(next_mm, cpu);
		next_mm->context[cpu] = mmc;
	}
#ifdef CONFIG_SMP
	else
		cpu_data[cpu].need_new_asn = 1;
#endif

	task_thread_info(next)->pcb.asn = mmc & HARDWARE_ASN_MASK;
}

__EXTERN_INLINE void
ev4_switch_mm(struct mm_struct *prev_mm, struct mm_struct *next_mm,
	      struct task_struct *next)
{
	if (prev_mm != next_mm)
		tbiap();

	ev5_switch_mm(prev_mm, next_mm, next);
}

extern void __load_new_mm_context(struct mm_struct *);

#ifdef CONFIG_SMP
#define check_mmu_context()					\
do {								\
	int cpu = smp_processor_id();				\
	cpu_data[cpu].asn_lock = 0;				\
	barrier();						\
	if (cpu_data[cpu].need_new_asn) {			\
		struct mm_struct * mm = current->active_mm;	\
		cpu_data[cpu].need_new_asn = 0;			\
		if (!mm->context[cpu])			\
			__load_new_mm_context(mm);		\
	}							\
} while(0)
#else
#define check_mmu_context()  do { } while(0)
#endif

__EXTERN_INLINE void
ev5_activate_mm(struct mm_struct *prev_mm, struct mm_struct *next_mm)
{
	__load_new_mm_context(next_mm);
}

__EXTERN_INLINE void
ev4_activate_mm(struct mm_struct *prev_mm, struct mm_struct *next_mm)
{
	__load_new_mm_context(next_mm);
	tbiap();
}

#define deactivate_mm(tsk,mm)	do { } while (0)

#ifdef CONFIG_ALPHA_GENERIC
# define switch_mm(a,b,c)	alpha_mv.mv_switch_mm((a),(b),(c))
# define activate_mm(x,y)	alpha_mv.mv_activate_mm((x),(y))
#else
# ifdef CONFIG_ALPHA_EV4
#  define switch_mm(a,b,c)	ev4_switch_mm((a),(b),(c))
#  define activate_mm(x,y)	ev4_activate_mm((x),(y))
# else
#  define switch_mm(a,b,c)	ev5_switch_mm((a),(b),(c))
#  define activate_mm(x,y)	ev5_activate_mm((x),(y))
# endif
#endif

static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	int i;

	for_each_online_cpu(i)
		mm->context[i] = 0;
	if (tsk != current)
		task_thread_info(tsk)->pcb.ptbr
		  = ((unsigned long)mm->pgd - IDENT_ADDR) >> PAGE_SHIFT;
	return 0;
}

extern inline void
destroy_context(struct mm_struct *mm)
{
	
}

static inline void
enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
	task_thread_info(tsk)->pcb.ptbr
	  = ((unsigned long)mm->pgd - IDENT_ADDR) >> PAGE_SHIFT;
}

#ifdef __MMU_EXTERN_INLINE
#undef __EXTERN_INLINE
#undef __MMU_EXTERN_INLINE
#endif

#endif 
