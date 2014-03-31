/*
 *  linux/arch/m32r/kernel/smp.c
 *
 *  M32R SMP support routines.
 *
 *  Copyright (c) 2001, 2002  Hitoshi Yamamoto
 *
 *  Taken from i386 version.
 *    (c) 1995 Alan Cox, Building #3 <alan@redhat.com>
 *    (c) 1998-99, 2000 Ingo Molnar <mingo@redhat.com>
 *
 *  This code is released under the GNU General Public License version 2 or
 *  later.
 */

#undef DEBUG_SMP

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/profile.h>
#include <linux/cpu.h>

#include <asm/cacheflush.h>
#include <asm/pgalloc.h>
#include <linux/atomic.h>
#include <asm/io.h>
#include <asm/mmu_context.h>
#include <asm/m32r.h>
#include <asm/tlbflush.h>


static DEFINE_SPINLOCK(flushcache_lock);
static volatile unsigned long flushcache_cpumask = 0;

static volatile cpumask_t flush_cpumask;
static struct mm_struct *flush_mm;
static struct vm_area_struct *flush_vma;
static volatile unsigned long flush_va;
static DEFINE_SPINLOCK(tlbstate_lock);
#define FLUSH_ALL 0xffffffff

DECLARE_PER_CPU(int, prof_multiplier);
DECLARE_PER_CPU(int, prof_old_multiplier);
DECLARE_PER_CPU(int, prof_counter);

extern spinlock_t ipi_lock[];


void smp_reschedule_interrupt(void);
void smp_flush_cache_all_interrupt(void);

static void flush_tlb_all_ipi(void *);
static void flush_tlb_others(cpumask_t, struct mm_struct *,
	struct vm_area_struct *, unsigned long);

void smp_invalidate_interrupt(void);

static void stop_this_cpu(void *);

void smp_ipi_timer_interrupt(struct pt_regs *);
void smp_local_timer_interrupt(void);

static void send_IPI_allbutself(int, int);
static void send_IPI_mask(const struct cpumask *, int, int);


void smp_send_reschedule(int cpu_id)
{
	WARN_ON(cpu_is_offline(cpu_id));
	send_IPI_mask(cpumask_of(cpu_id), RESCHEDULE_IPI, 1);
}

void smp_reschedule_interrupt(void)
{
	scheduler_ipi();
}

void smp_flush_cache_all(void)
{
	cpumask_t cpumask;
	unsigned long *mask;

	preempt_disable();
	cpumask_copy(&cpumask, cpu_online_mask);
	cpumask_clear_cpu(smp_processor_id(), &cpumask);
	spin_lock(&flushcache_lock);
	mask=cpumask_bits(&cpumask);
	atomic_set_mask(*mask, (atomic_t *)&flushcache_cpumask);
	send_IPI_mask(&cpumask, INVALIDATE_CACHE_IPI, 0);
	_flush_cache_copyback_all();
	while (flushcache_cpumask)
		mb();
	spin_unlock(&flushcache_lock);
	preempt_enable();
}

void smp_flush_cache_all_interrupt(void)
{
	_flush_cache_copyback_all();
	clear_bit(smp_processor_id(), &flushcache_cpumask);
}


void smp_flush_tlb_all(void)
{
	unsigned long flags;

	preempt_disable();
	local_irq_save(flags);
	__flush_tlb_all();
	local_irq_restore(flags);
	smp_call_function(flush_tlb_all_ipi, NULL, 1);
	preempt_enable();
}

static void flush_tlb_all_ipi(void *info)
{
	__flush_tlb_all();
}

void smp_flush_tlb_mm(struct mm_struct *mm)
{
	int cpu_id;
	cpumask_t cpu_mask;
	unsigned long *mmc;
	unsigned long flags;

	preempt_disable();
	cpu_id = smp_processor_id();
	mmc = &mm->context[cpu_id];
	cpumask_copy(&cpu_mask, mm_cpumask(mm));
	cpumask_clear_cpu(cpu_id, &cpu_mask);

	if (*mmc != NO_CONTEXT) {
		local_irq_save(flags);
		*mmc = NO_CONTEXT;
		if (mm == current->mm)
			activate_context(mm);
		else
			cpumask_clear_cpu(cpu_id, mm_cpumask(mm));
		local_irq_restore(flags);
	}
	if (!cpumask_empty(&cpu_mask))
		flush_tlb_others(cpu_mask, mm, NULL, FLUSH_ALL);

	preempt_enable();
}

void smp_flush_tlb_range(struct vm_area_struct *vma, unsigned long start,
	unsigned long end)
{
	smp_flush_tlb_mm(vma->vm_mm);
}

void smp_flush_tlb_page(struct vm_area_struct *vma, unsigned long va)
{
	struct mm_struct *mm = vma->vm_mm;
	int cpu_id;
	cpumask_t cpu_mask;
	unsigned long *mmc;
	unsigned long flags;

	preempt_disable();
	cpu_id = smp_processor_id();
	mmc = &mm->context[cpu_id];
	cpumask_copy(&cpu_mask, mm_cpumask(mm));
	cpumask_clear_cpu(cpu_id, &cpu_mask);

#ifdef DEBUG_SMP
	if (!mm)
		BUG();
#endif

	if (*mmc != NO_CONTEXT) {
		local_irq_save(flags);
		va &= PAGE_MASK;
		va |= (*mmc & MMU_CONTEXT_ASID_MASK);
		__flush_tlb_page(va);
		local_irq_restore(flags);
	}
	if (!cpumask_empty(&cpu_mask))
		flush_tlb_others(cpu_mask, mm, vma, va);

	preempt_enable();
}

static void flush_tlb_others(cpumask_t cpumask, struct mm_struct *mm,
	struct vm_area_struct *vma, unsigned long va)
{
	unsigned long *mask;
#ifdef DEBUG_SMP
	unsigned long flags;
	__save_flags(flags);
	if (!(flags & 0x0040))	
		BUG();
#endif 

	BUG_ON(cpumask_empty(&cpumask));

	BUG_ON(cpumask_test_cpu(smp_processor_id(), &cpumask));
	BUG_ON(!mm);

	
	cpumask_and(&cpumask, &cpumask, cpu_online_mask);
	if (cpumask_empty(&cpumask))
		return;

	spin_lock(&tlbstate_lock);

	flush_mm = mm;
	flush_vma = vma;
	flush_va = va;
	mask=cpumask_bits(&cpumask);
	atomic_set_mask(*mask, (atomic_t *)&flush_cpumask);

	send_IPI_mask(&cpumask, INVALIDATE_TLB_IPI, 0);

	while (!cpumask_empty((cpumask_t*)&flush_cpumask)) {
		
		mb();
	}

	flush_mm = NULL;
	flush_vma = NULL;
	flush_va = 0;
	spin_unlock(&tlbstate_lock);
}

void smp_invalidate_interrupt(void)
{
	int cpu_id = smp_processor_id();
	unsigned long *mmc = &flush_mm->context[cpu_id];

	if (!cpumask_test_cpu(cpu_id, &flush_cpumask))
		return;

	if (flush_va == FLUSH_ALL) {
		*mmc = NO_CONTEXT;
		if (flush_mm == current->active_mm)
			activate_context(flush_mm);
		else
			cpumask_clear_cpu(cpu_id, mm_cpumask(flush_mm));
	} else {
		unsigned long va = flush_va;

		if (*mmc != NO_CONTEXT) {
			va &= PAGE_MASK;
			va |= (*mmc & MMU_CONTEXT_ASID_MASK);
			__flush_tlb_page(va);
		}
	}
	cpumask_clear_cpu(cpu_id, (cpumask_t*)&flush_cpumask);
}


void smp_send_stop(void)
{
	smp_call_function(stop_this_cpu, NULL, 0);
}

static void stop_this_cpu(void *dummy)
{
	int cpu_id = smp_processor_id();

	set_cpu_online(cpu_id, false);

	local_irq_disable();
	outl(0, M32R_ICU_IMASK_PORTL);
	inl(M32R_ICU_IMASK_PORTL);	
	local_irq_enable();

	for ( ; ; );
}

void arch_send_call_function_ipi_mask(const struct cpumask *mask)
{
	send_IPI_mask(mask, CALL_FUNCTION_IPI, 0);
}

void arch_send_call_function_single_ipi(int cpu)
{
	send_IPI_mask(cpumask_of(cpu), CALL_FUNC_SINGLE_IPI, 0);
}

void smp_call_function_interrupt(void)
{
	irq_enter();
	generic_smp_call_function_interrupt();
	irq_exit();
}

void smp_call_function_single_interrupt(void)
{
	irq_enter();
	generic_smp_call_function_single_interrupt();
	irq_exit();
}


void smp_send_timer(void)
{
	send_IPI_allbutself(LOCAL_TIMER_IPI, 1);
}

void smp_ipi_timer_interrupt(struct pt_regs *regs)
{
	struct pt_regs *old_regs;
	old_regs = set_irq_regs(regs);
	irq_enter();
	smp_local_timer_interrupt();
	irq_exit();
	set_irq_regs(old_regs);
}

void smp_local_timer_interrupt(void)
{
	int user = user_mode(get_irq_regs());
	int cpu_id = smp_processor_id();


	profile_tick(CPU_PROFILING);

	if (--per_cpu(prof_counter, cpu_id) <= 0) {
		per_cpu(prof_counter, cpu_id)
			= per_cpu(prof_multiplier, cpu_id);
		if (per_cpu(prof_counter, cpu_id)
			!= per_cpu(prof_old_multiplier, cpu_id))
		{
			per_cpu(prof_old_multiplier, cpu_id)
				= per_cpu(prof_counter, cpu_id);
		}

		update_process_times(user);
	}
}


static void send_IPI_allbutself(int ipi_num, int try)
{
	cpumask_t cpumask;

	cpumask_copy(&cpumask, cpu_online_mask);
	cpumask_clear_cpu(smp_processor_id(), &cpumask);

	send_IPI_mask(&cpumask, ipi_num, try);
}

static void send_IPI_mask(const struct cpumask *cpumask, int ipi_num, int try)
{
	cpumask_t physid_mask, tmp;
	int cpu_id, phys_id;
	int num_cpus = num_online_cpus();

	if (num_cpus <= 1)	
		return;

	cpumask_and(&tmp, cpumask, cpu_online_mask);
	BUG_ON(!cpumask_equal(cpumask, &tmp));

	cpumask_clear(&physid_mask);
	for_each_cpu(cpu_id, cpumask) {
		if ((phys_id = cpu_to_physid(cpu_id)) != -1)
			cpumask_set_cpu(phys_id, &physid_mask);
	}

	send_IPI_mask_phys(&physid_mask, ipi_num, try);
}

unsigned long send_IPI_mask_phys(const cpumask_t *physid_mask, int ipi_num,
	int try)
{
	spinlock_t *ipilock;
	volatile unsigned long *ipicr_addr;
	unsigned long ipicr_val;
	unsigned long my_physid_mask;
	unsigned long mask = cpumask_bits(physid_mask)[0];


	if (mask & ~physids_coerce(phys_cpu_present_map))
		BUG();
	if (ipi_num >= NR_IPIS || ipi_num < 0)
		BUG();

	mask <<= IPI_SHIFT;
	ipilock = &ipi_lock[ipi_num];
	ipicr_addr = (volatile unsigned long *)(M32R_ICU_IPICR_ADDR
		+ (ipi_num << 2));
	my_physid_mask = ~(1 << smp_processor_id());

	spin_lock(ipilock);
	__asm__ __volatile__ (
		";; CHECK IPICRi == 0		\n\t"
		".fillinsn			\n"
		"1:				\n\t"
		"ld	%0, @%1			\n\t"
		"and	%0, %4			\n\t"
		"beqz	%0, 2f			\n\t"
		"bnez	%3, 3f			\n\t"
		"bra	1b			\n\t"
		";; WRITE IPICRi (send IPIi)	\n\t"
		".fillinsn			\n"
		"2:				\n\t"
		"st	%2, @%1			\n\t"
		".fillinsn			\n"
		"3:				\n\t"
		: "=&r"(ipicr_val)
		: "r"(ipicr_addr), "r"(mask), "r"(try), "r"(my_physid_mask)
		: "memory"
	);
	spin_unlock(ipilock);

	return ipicr_val;
}
