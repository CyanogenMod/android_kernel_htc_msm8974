#include <linux/types.h>
#include <asm/delay.h>
#include <irq.h>
#include <hwregs/intr_vect.h>
#include <hwregs/intr_vect_defs.h>
#include <asm/tlbflush.h>
#include <asm/mmu_context.h>
#include <hwregs/asm/mmu_defs_asm.h>
#include <hwregs/supp_reg.h>
#include <linux/atomic.h>

#include <linux/err.h>
#include <linux/init.h>
#include <linux/timex.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#define IPI_SCHEDULE 1
#define IPI_CALL 2
#define IPI_FLUSH_TLB 4
#define IPI_BOOT 8

#define FLUSH_ALL (void*)0xffffffff

spinlock_t cris_atomic_locks[] = {
	[0 ... LOCK_COUNT - 1] = __SPIN_LOCK_UNLOCKED(cris_atomic_locks)
};

cpumask_t phys_cpu_present_map = CPU_MASK_NONE;
EXPORT_SYMBOL(phys_cpu_present_map);

volatile int cpu_now_booting = 0;
volatile struct thread_info *smp_init_current_idle_thread;

static DEFINE_SPINLOCK(call_lock);
static DEFINE_SPINLOCK(tlbstate_lock);

struct call_data_struct {
	void (*func) (void *info);
	void *info;
	int wait;
};

static struct call_data_struct * call_data;

static struct mm_struct* flush_mm;
static struct vm_area_struct* flush_vma;
static unsigned long flush_addr;

static unsigned long irq_regs[NR_CPUS] = {
  regi_irq,
  regi_irq2
};

static irqreturn_t crisv32_ipi_interrupt(int irq, void *dev_id);
static int send_ipi(int vector, int wait, cpumask_t cpu_mask);
static struct irqaction irq_ipi  = {
	.handler = crisv32_ipi_interrupt,
	.flags = IRQF_DISABLED,
	.name = "ipi",
};

extern void cris_mmu_init(void);
extern void cris_timer_init(void);

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	int i;

	
	setup_irq(IPI_INTR_VECT, &irq_ipi);

	
	for (i = 0; i < max_cpus; i++)
		cpumask_set_cpu(i, &phys_cpu_present_map);
}

void __devinit smp_prepare_boot_cpu(void)
{
  	pgd_t **pgd;
	pgd = (pgd_t**)&per_cpu(current_pgd, smp_processor_id());

	SUPP_BANK_SEL(1);
	SUPP_REG_WR(RW_MM_TLB_PGD, pgd);
	SUPP_BANK_SEL(2);
	SUPP_REG_WR(RW_MM_TLB_PGD, pgd);

	set_cpu_online(0, true);
	cpumask_set_cpu(0, &phys_cpu_present_map);
	set_cpu_possible(0, true);
}

void __init smp_cpus_done(unsigned int max_cpus)
{
}

static int __init
smp_boot_one_cpu(int cpuid)
{
	unsigned timeout;
	struct task_struct *idle;
	cpumask_t cpu_mask;

	cpumask_clear(&cpu_mask);
	idle = fork_idle(cpuid);
	if (IS_ERR(idle))
		panic("SMP: fork failed for CPU:%d", cpuid);

	task_thread_info(idle)->cpu = cpuid;

	
	smp_init_current_idle_thread = task_thread_info(idle);
	cpu_now_booting = cpuid;

	
	set_cpu_online(cpuid, true);
	cpumask_set_cpu(cpuid, &cpu_mask);
	send_ipi(IPI_BOOT, 0, cpu_mask);
	set_cpu_online(cpuid, false);

	
	for (timeout = 0; timeout < 10000; timeout++) {
		if(cpu_online(cpuid)) {
			cpu_now_booting = 0;
			smp_init_current_idle_thread = NULL;
			return 0; 
		}
		udelay(100);
		barrier();
	}

	put_task_struct(idle);
	idle = NULL;

	printk(KERN_CRIT "SMP: CPU:%d is stuck.\n", cpuid);
	return -1;
}

void __init smp_callin(void)
{
	extern void cpu_idle(void);

	int cpu = cpu_now_booting;
	reg_intr_vect_rw_mask vect_mask = {0};

	
	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;

	
	cris_mmu_init();
	__flush_tlb_all();

	
	cris_timer_init();

	
	REG_WR(intr_vect, irq_regs[cpu], rw_mask, vect_mask);
	crisv32_unmask_irq(IPI_INTR_VECT);
	crisv32_unmask_irq(TIMER0_INTR_VECT);
	preempt_disable();
	notify_cpu_starting(cpu);
	local_irq_enable();

	set_cpu_online(cpu, true);
	cpu_idle();
}

void stop_this_cpu(void* dummy)
{
	local_irq_disable();
	asm volatile("halt");
}

void smp_send_stop(void)
{
	smp_call_function(stop_this_cpu, NULL, 0);
}

int setup_profiling_timer(unsigned int multiplier)
{
	return -EINVAL;
}


unsigned long cache_decay_ticks = 1;

int __cpuinit __cpu_up(unsigned int cpu)
{
	smp_boot_one_cpu(cpu);
	return cpu_online(cpu) ? 0 : -ENOSYS;
}

void smp_send_reschedule(int cpu)
{
	cpumask_t cpu_mask;
	cpumask_clear(&cpu_mask);
	cpumask_set_cpu(cpu, &cpu_mask);
	send_ipi(IPI_SCHEDULE, 0, cpu_mask);
}

void flush_tlb_common(struct mm_struct* mm, struct vm_area_struct* vma, unsigned long addr)
{
	unsigned long flags;
	cpumask_t cpu_mask;

	spin_lock_irqsave(&tlbstate_lock, flags);
	cpu_mask = (mm == FLUSH_ALL ? cpu_all_mask : *mm_cpumask(mm));
	cpumask_clear_cpu(smp_processor_id(), &cpu_mask);
	flush_mm = mm;
	flush_vma = vma;
	flush_addr = addr;
	send_ipi(IPI_FLUSH_TLB, 1, cpu_mask);
	spin_unlock_irqrestore(&tlbstate_lock, flags);
}

void flush_tlb_all(void)
{
	__flush_tlb_all();
	flush_tlb_common(FLUSH_ALL, FLUSH_ALL, 0);
}

void flush_tlb_mm(struct mm_struct *mm)
{
	__flush_tlb_mm(mm);
	flush_tlb_common(mm, FLUSH_ALL, 0);
	
	cpumask_clear(mm_cpumask(mm));
	cpumask_set_cpu(smp_processor_id(), mm_cpumask(mm));
}

void flush_tlb_page(struct vm_area_struct *vma,
			   unsigned long addr)
{
	__flush_tlb_page(vma, addr);
	flush_tlb_common(vma->vm_mm, vma, addr);
}


int send_ipi(int vector, int wait, cpumask_t cpu_mask)
{
	int i = 0;
	reg_intr_vect_rw_ipi ipi = REG_RD(intr_vect, irq_regs[i], rw_ipi);
	int ret = 0;

	
	cpumask_and(&cpu_mask, &cpu_mask, cpu_online_mask);

	
	for_each_cpu(i, &cpu_mask)
	{
		ipi.vector |= vector;
		REG_WR(intr_vect, irq_regs[i], rw_ipi, ipi);
	}

	
	if (wait) {
		for_each_cpu(i, &cpu_mask) {
                        int j;
                        for (j = 0 ; j < 1000; j++) {
				ipi = REG_RD(intr_vect, irq_regs[i], rw_ipi);
				if (!ipi.vector)
					break;
				udelay(100);
			}

			
			if (ipi.vector) {
				printk("SMP call timeout from %d to %d\n", smp_processor_id(), i);
				ret = -ETIMEDOUT;
				dump_stack();
			}
		}
	}
	return ret;
}

int smp_call_function(void (*func)(void *info), void *info, int wait)
{
	cpumask_t cpu_mask;
	struct call_data_struct data;
	int ret;

	cpumask_setall(&cpu_mask);
	cpumask_clear_cpu(smp_processor_id(), &cpu_mask);

	WARN_ON(irqs_disabled());

	data.func = func;
	data.info = info;
	data.wait = wait;

	spin_lock(&call_lock);
	call_data = &data;
	ret = send_ipi(IPI_CALL, wait, cpu_mask);
	spin_unlock(&call_lock);

	return ret;
}

irqreturn_t crisv32_ipi_interrupt(int irq, void *dev_id)
{
	void (*func) (void *info) = call_data->func;
	void *info = call_data->info;
	reg_intr_vect_rw_ipi ipi;

	ipi = REG_RD(intr_vect, irq_regs[smp_processor_id()], rw_ipi);

	if (ipi.vector & IPI_SCHEDULE) {
		scheduler_ipi();
	}
	if (ipi.vector & IPI_CALL) {
		func(info);
	}
	if (ipi.vector & IPI_FLUSH_TLB) {
		if (flush_mm == FLUSH_ALL)
			__flush_tlb_all();
		else if (flush_vma == FLUSH_ALL)
			__flush_tlb_mm(flush_mm);
		else
			__flush_tlb_page(flush_vma, flush_addr);
	}

	ipi.vector = 0;
	REG_WR(intr_vect, irq_regs[smp_processor_id()], rw_ipi, ipi);

	return IRQ_HANDLED;
}

