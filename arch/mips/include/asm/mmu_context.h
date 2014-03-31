/*
 * Switch a MMU context.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 1998, 1999 by Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#ifndef _ASM_MMU_CONTEXT_H
#define _ASM_MMU_CONTEXT_H

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/hazards.h>
#include <asm/tlbflush.h>
#ifdef CONFIG_MIPS_MT_SMTC
#include <asm/mipsmtregs.h>
#include <asm/smtc.h>
#endif 
#include <asm-generic/mm_hooks.h>

#ifdef CONFIG_MIPS_PGD_C0_CONTEXT

#define TLBMISS_HANDLER_SETUP_PGD(pgd)				\
	tlbmiss_handler_setup_pgd((unsigned long)(pgd))

extern void tlbmiss_handler_setup_pgd(unsigned long pgd);

#define TLBMISS_HANDLER_SETUP()						\
	do {								\
		TLBMISS_HANDLER_SETUP_PGD(swapper_pg_dir);		\
		write_c0_xcontext((unsigned long) smp_processor_id() << 51); \
	} while (0)

#else 

extern unsigned long pgd_current[];

#define TLBMISS_HANDLER_SETUP_PGD(pgd) \
	pgd_current[smp_processor_id()] = (unsigned long)(pgd)

#ifdef CONFIG_32BIT
#define TLBMISS_HANDLER_SETUP()						\
	write_c0_context((unsigned long) smp_processor_id() << 25);	\
	back_to_back_c0_hazard();					\
	TLBMISS_HANDLER_SETUP_PGD(swapper_pg_dir)
#endif
#ifdef CONFIG_64BIT
#define TLBMISS_HANDLER_SETUP()						\
	write_c0_context((unsigned long) smp_processor_id() << 26);	\
	back_to_back_c0_hazard();					\
	TLBMISS_HANDLER_SETUP_PGD(swapper_pg_dir)
#endif
#endif 
#if defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_TX39XX)

#define ASID_INC	0x40
#define ASID_MASK	0xfc0

#elif defined(CONFIG_CPU_R8000)

#define ASID_INC	0x10
#define ASID_MASK	0xff0

#elif defined(CONFIG_CPU_RM9000)

#define ASID_INC	0x1
#define ASID_MASK	0xfff

#elif defined(CONFIG_MIPS_MT_SMTC)

#define ASID_INC	0x1
extern unsigned long smtc_asid_mask;
#define ASID_MASK	(smtc_asid_mask)
#define	HW_ASID_MASK	0xff
#else 

#define ASID_INC	0x1
#define ASID_MASK	0xff

#endif

#define cpu_context(cpu, mm)	((mm)->context.asid[cpu])
#define cpu_asid(cpu, mm)	(cpu_context((cpu), (mm)) & ASID_MASK)
#define asid_cache(cpu)		(cpu_data[cpu].asid_cache)

static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
}

#define ASID_VERSION_MASK  ((unsigned long)~(ASID_MASK|(ASID_MASK-1)))
#define ASID_FIRST_VERSION ((unsigned long)(~ASID_VERSION_MASK) + 1)

#ifndef CONFIG_MIPS_MT_SMTC
static inline void
get_new_mmu_context(struct mm_struct *mm, unsigned long cpu)
{
	unsigned long asid = asid_cache(cpu);

	if (! ((asid += ASID_INC) & ASID_MASK) ) {
		if (cpu_has_vtag_icache)
			flush_icache_all();
		local_flush_tlb_all();	
		if (!asid)		
			asid = ASID_FIRST_VERSION;
	}
	cpu_context(cpu, mm) = asid_cache(cpu) = asid;
}

#else 

#define get_new_mmu_context(mm, cpu) smtc_get_new_mmu_context((mm), (cpu))

#endif 

static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	int i;

	for_each_online_cpu(i)
		cpu_context(i, mm) = 0;

	return 0;
}

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
                             struct task_struct *tsk)
{
	unsigned int cpu = smp_processor_id();
	unsigned long flags;
#ifdef CONFIG_MIPS_MT_SMTC
	unsigned long oldasid;
	unsigned long mtflags;
	int mytlb = (smtc_status & SMTC_TLB_SHARED) ? 0 : cpu_data[cpu].vpe_id;
	local_irq_save(flags);
	mtflags = dvpe();
#else 
	local_irq_save(flags);
#endif 

	
	if ((cpu_context(cpu, next) ^ asid_cache(cpu)) & ASID_VERSION_MASK)
		get_new_mmu_context(next, cpu);
#ifdef CONFIG_MIPS_MT_SMTC
	oldasid = (read_c0_entryhi() & ASID_MASK);
	if(smtc_live_asid[mytlb][oldasid]) {
		smtc_live_asid[mytlb][oldasid] &= ~(0x1 << cpu);
		if(smtc_live_asid[mytlb][oldasid] == 0)
			smtc_flush_tlb_asid(oldasid);
	}
	write_c0_entryhi((read_c0_entryhi() & ~HW_ASID_MASK) |
			 cpu_asid(cpu, next));
	ehb(); 
	evpe(mtflags);
#else
	write_c0_entryhi(cpu_asid(cpu, next));
#endif 
	TLBMISS_HANDLER_SETUP_PGD(next->pgd);

	cpumask_clear_cpu(cpu, mm_cpumask(prev));
	cpumask_set_cpu(cpu, mm_cpumask(next));

	local_irq_restore(flags);
}

static inline void destroy_context(struct mm_struct *mm)
{
}

#define deactivate_mm(tsk, mm)	do { } while (0)

static inline void
activate_mm(struct mm_struct *prev, struct mm_struct *next)
{
	unsigned long flags;
	unsigned int cpu = smp_processor_id();

#ifdef CONFIG_MIPS_MT_SMTC
	unsigned long oldasid;
	unsigned long mtflags;
	int mytlb = (smtc_status & SMTC_TLB_SHARED) ? 0 : cpu_data[cpu].vpe_id;
#endif 

	local_irq_save(flags);

	
	get_new_mmu_context(next, cpu);

#ifdef CONFIG_MIPS_MT_SMTC
	
	mtflags = dvpe();
	oldasid = read_c0_entryhi() & ASID_MASK;
	if(smtc_live_asid[mytlb][oldasid]) {
		smtc_live_asid[mytlb][oldasid] &= ~(0x1 << cpu);
		if(smtc_live_asid[mytlb][oldasid] == 0)
			 smtc_flush_tlb_asid(oldasid);
	}
	
	write_c0_entryhi((read_c0_entryhi() & ~HW_ASID_MASK) |
	                 cpu_asid(cpu, next));
	ehb(); 
	evpe(mtflags);
#else
	write_c0_entryhi(cpu_asid(cpu, next));
#endif 
	TLBMISS_HANDLER_SETUP_PGD(next->pgd);

	
	cpumask_clear_cpu(cpu, mm_cpumask(prev));
	cpumask_set_cpu(cpu, mm_cpumask(next));

	local_irq_restore(flags);
}

static inline void
drop_mmu_context(struct mm_struct *mm, unsigned cpu)
{
	unsigned long flags;
#ifdef CONFIG_MIPS_MT_SMTC
	unsigned long oldasid;
	
	unsigned int prevvpe;
	int mytlb = (smtc_status & SMTC_TLB_SHARED) ? 0 : cpu_data[cpu].vpe_id;
#endif 

	local_irq_save(flags);

	if (cpumask_test_cpu(cpu, mm_cpumask(mm)))  {
		get_new_mmu_context(mm, cpu);
#ifdef CONFIG_MIPS_MT_SMTC
		
		prevvpe = dvpe();
		oldasid = (read_c0_entryhi() & ASID_MASK);
		if (smtc_live_asid[mytlb][oldasid]) {
			smtc_live_asid[mytlb][oldasid] &= ~(0x1 << cpu);
			if(smtc_live_asid[mytlb][oldasid] == 0)
				smtc_flush_tlb_asid(oldasid);
		}
		
		write_c0_entryhi((read_c0_entryhi() & ~HW_ASID_MASK)
				| cpu_asid(cpu, mm));
		ehb(); 
		evpe(prevvpe);
#else 
		write_c0_entryhi(cpu_asid(cpu, mm));
#endif 
	} else {
		
#ifndef CONFIG_MIPS_MT_SMTC
		cpu_context(cpu, mm) = 0;
#else 
		int i;

		
		for_each_online_cpu(i) {
		    if((smtc_status & SMTC_TLB_SHARED)
		    || (cpu_data[i].vpe_id == cpu_data[cpu].vpe_id))
			cpu_context(i, mm) = 0;
		}
#endif 
	}
	local_irq_restore(flags);
}

#endif 
