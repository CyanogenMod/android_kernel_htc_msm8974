/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Copyright (C) 2004 Mips Technologies, Inc
 * Copyright (C) 2008 Kevin D. Kissell
 */

#include <linux/clockchips.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/ftrace.h>
#include <linux/slab.h>

#include <asm/cpu.h>
#include <asm/processor.h>
#include <linux/atomic.h>
#include <asm/hardirq.h>
#include <asm/hazards.h>
#include <asm/irq.h>
#include <asm/mmu_context.h>
#include <asm/mipsregs.h>
#include <asm/cacheflush.h>
#include <asm/time.h>
#include <asm/addrspace.h>
#include <asm/smtc.h>
#include <asm/smtc_proc.h>

unsigned long irq_hwmask[NR_IRQS];

#define LOCK_MT_PRA() \
	local_irq_save(flags); \
	mtflags = dmt()

#define UNLOCK_MT_PRA() \
	emt(mtflags); \
	local_irq_restore(flags)

#define LOCK_CORE_PRA() \
	local_irq_save(flags); \
	mtflags = dvpe()

#define UNLOCK_CORE_PRA() \
	evpe(mtflags); \
	local_irq_restore(flags)




asiduse smtc_live_asid[MAX_SMTC_TLBS][MAX_SMTC_ASIDS];


#define IPIBUF_PER_CPU 4

struct smtc_ipi_q IPIQ[NR_CPUS];
static struct smtc_ipi_q freeIPIq;



void ipi_decode(struct smtc_ipi *);
static void post_direct_ipi(int cpu, struct smtc_ipi *pipi);
static void setup_cross_vpe_interrupts(unsigned int nvpe);
void init_smtc_stats(void);


unsigned int smtc_status;


static int vpe0limit;
static int ipibuffers;
static int nostlb;
static int asidmask;
unsigned long smtc_asid_mask = 0xff;

static int __init vpe0tcs(char *str)
{
	get_option(&str, &vpe0limit);

	return 1;
}

static int __init ipibufs(char *str)
{
	get_option(&str, &ipibuffers);
	return 1;
}

static int __init stlb_disable(char *s)
{
	nostlb = 1;
	return 1;
}

static int __init asidmask_set(char *str)
{
	get_option(&str, &asidmask);
	switch (asidmask) {
	case 0x1:
	case 0x3:
	case 0x7:
	case 0xf:
	case 0x1f:
	case 0x3f:
	case 0x7f:
	case 0xff:
		smtc_asid_mask = (unsigned long)asidmask;
		break;
	default:
		printk("ILLEGAL ASID mask 0x%x from command line\n", asidmask);
	}
	return 1;
}

__setup("vpe0tcs=", vpe0tcs);
__setup("ipibufs=", ipibufs);
__setup("nostlb", stlb_disable);
__setup("asidmask=", asidmask_set);

#ifdef CONFIG_SMTC_IDLE_HOOK_DEBUG

static int hang_trig;

static int __init hangtrig_enable(char *s)
{
	hang_trig = 1;
	return 1;
}


__setup("hangtrig", hangtrig_enable);

#define DEFAULT_BLOCKED_IPI_LIMIT 32

static int timerq_limit = DEFAULT_BLOCKED_IPI_LIMIT;

static int __init tintq(char *str)
{
	get_option(&str, &timerq_limit);
	return 1;
}

__setup("tintq=", tintq);

static int imstuckcount[2][8];
static int vpemask[2][8] = {
	{0, 0, 1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 1}
};
int tcnoprog[NR_CPUS];
static atomic_t idle_hook_initialized = ATOMIC_INIT(0);
static int clock_hang_reported[NR_CPUS];

#endif 


static void smtc_configure_tlb(void)
{
	int i, tlbsiz, vpes;
	unsigned long mvpconf0;
	unsigned long config1val;

	
	for (vpes=0; vpes<MAX_SMTC_TLBS; vpes++) {
	    for(i = 0; i < MAX_SMTC_ASIDS; i++) {
		smtc_live_asid[vpes][i] = 0;
	    }
	}
	mvpconf0 = read_c0_mvpconf0();

	if ((vpes = ((mvpconf0 & MVPCONF0_PVPE)
			>> MVPCONF0_PVPE_SHIFT) + 1) > 1) {
	    
	    if ((mvpconf0 & MVPCONF0_TLBS) && !nostlb) {
		if ((tlbsiz = ((mvpconf0 & MVPCONF0_PTLBE)
				>> MVPCONF0_PTLBE_SHIFT)) == 0) {
		    settc(1);
		    
		    write_tc_c0_tchalt(TCHALT_H);
		    mips_ihb();
		    
		    for (i=0; i < vpes; i++) {
		    	write_tc_c0_tcbind(i);
			write_c0_mvpcontrol(
				read_c0_mvpcontrol() & ~ MVPCONTROL_VPC );
			mips_ihb();
			if (((read_vpe_c0_config() & MIPS_CONF_MT) >> 7) == 1) {
				config1val = read_vpe_c0_config1();
				tlbsiz += ((config1val >> 25) & 0x3f) + 1;
			}

			
			write_c0_mvpcontrol(
				read_c0_mvpcontrol() | MVPCONTROL_VPC );
			mips_ihb();
		    }
		}
		write_c0_mvpcontrol(read_c0_mvpcontrol() | MVPCONTROL_STLB);
		ehb();


		
		if (tlbsiz > 64)
			tlbsiz = 64;
		cpu_data[0].tlbsize = current_cpu_data.tlbsize = tlbsiz;
		smtc_status |= SMTC_TLB_SHARED;
		local_flush_tlb_all();

		printk("TLB of %d entry pairs shared by %d VPEs\n",
			tlbsiz, vpes);
	    } else {
		printk("WARNING: TLB Not Sharable on SMTC Boot!\n");
	    }
	}
}



int __init smtc_build_cpu_map(int start_cpu_slot)
{
	int i, ntcs;

	ntcs = ((read_c0_mvpconf0() & MVPCONF0_PTC) >> MVPCONF0_PTC_SHIFT) + 1;
	for (i=start_cpu_slot; i<NR_CPUS && i<ntcs; i++) {
		set_cpu_possible(i, true);
		__cpu_number_map[i] = i;
		__cpu_logical_map[i] = i;
	}
#ifdef CONFIG_MIPS_MT_FPAFF
	
	cpus_clear(mt_fpu_cpumask);
#endif

	
	printk("%i available secondary CPU TC(s)\n", i - 1);

	return i;
}


static void smtc_tc_setup(int vpe, int tc, int cpu)
{
	settc(tc);
	write_tc_c0_tchalt(TCHALT_H);
	mips_ihb();
	write_tc_c0_tcstatus((read_tc_c0_tcstatus()
			& ~(TCSTATUS_TKSU | TCSTATUS_DA | TCSTATUS_IXMT))
			| TCSTATUS_A);
	write_tc_c0_tccontext((sizeof(struct smtc_ipi_q) * cpu) << 16);
	
	write_tc_c0_tcbind(vpe);
	
	memcpy(&cpu_data[cpu], &cpu_data[0], sizeof(struct cpuinfo_mips));
	
	if (cpu_data[0].cputype == CPU_34K ||
	    cpu_data[0].cputype == CPU_1004K)
		cpu_data[cpu].options &= ~MIPS_CPU_FPU;
	cpu_data[cpu].vpe_id = vpe;
	cpu_data[cpu].tc_id = tc;
	
	cpu_data[cpu].core = (read_vpe_c0_ebase() >> 1) & 0xff;
}


#define CP0_SKEW 8

void smtc_prepare_cpus(int cpus)
{
	int i, vpe, tc, ntc, nvpe, tcpervpe[NR_CPUS], slop, cpu;
	unsigned long flags;
	unsigned long val;
	int nipi;
	struct smtc_ipi *pipi;

	
	local_irq_save(flags);
	
	dvpe();
	dmt();

	spin_lock_init(&freeIPIq.lock);

	for (i=0; i<NR_CPUS; i++) {
		IPIQ[i].head = IPIQ[i].tail = NULL;
		spin_lock_init(&IPIQ[i].lock);
		IPIQ[i].depth = 0;
		IPIQ[i].resched_flag = 0; 
	}

	
	cpu = 0;
	cpu_data[cpu].vpe_id = 0;
	cpu_data[cpu].tc_id = 0;
	cpu_data[cpu].core = (read_c0_ebase() >> 1) & 0xff;
	cpu++;

	
	mips_mt_set_cpuoptions();
	if (vpelimit > 0)
		printk("Limit of %d VPEs set\n", vpelimit);
	if (tclimit > 0)
		printk("Limit of %d TCs set\n", tclimit);
	if (nostlb) {
		printk("Shared TLB Use Inhibited - UNSAFE for Multi-VPE Operation\n");
	}
	if (asidmask)
		printk("ASID mask value override to 0x%x\n", asidmask);

	
#ifdef CONFIG_SMTC_IDLE_HOOK_DEBUG
	if (hang_trig)
		printk("Logic Analyser Trigger on suspected TC hang\n");
#endif 

	
	write_c0_mvpcontrol( read_c0_mvpcontrol() | MVPCONTROL_VPC );

	val = read_c0_mvpconf0();
	nvpe = ((val & MVPCONF0_PVPE) >> MVPCONF0_PVPE_SHIFT) + 1;
	if (vpelimit > 0 && nvpe > vpelimit)
		nvpe = vpelimit;
	ntc = ((val & MVPCONF0_PTC) >> MVPCONF0_PTC_SHIFT) + 1;
	if (ntc > NR_CPUS)
		ntc = NR_CPUS;
	if (tclimit > 0 && ntc > tclimit)
		ntc = tclimit;
	slop = ntc % nvpe;
	for (i = 0; i < nvpe; i++) {
		tcpervpe[i] = ntc / nvpe;
		if (slop) {
			if((slop - i) > 0) tcpervpe[i]++;
		}
	}
	
	if (vpe0limit > ntc) vpe0limit = ntc;
	if (vpe0limit > 0) {
		int slopslop;
		if (vpe0limit < tcpervpe[0]) {
		    
		    slop = tcpervpe[0] - vpe0limit;
		    slopslop = slop % (nvpe - 1);
		    tcpervpe[0] = vpe0limit;
		    for (i = 1; i < nvpe; i++) {
			tcpervpe[i] += slop / (nvpe - 1);
			if(slopslop && ((slopslop - (i - 1) > 0)))
				tcpervpe[i]++;
		    }
		} else if (vpe0limit > tcpervpe[0]) {
		    
		    slop = vpe0limit - tcpervpe[0];
		    slopslop = slop % (nvpe - 1);
		    tcpervpe[0] = vpe0limit;
		    for (i = 1; i < nvpe; i++) {
			tcpervpe[i] -= slop / (nvpe - 1);
			if(slopslop && ((slopslop - (i - 1) > 0)))
				tcpervpe[i]--;
		    }
		}
	}

	
	smtc_configure_tlb();

	for (tc = 0, vpe = 0 ; (vpe < nvpe) && (tc < ntc) ; vpe++) {
		if (tcpervpe[vpe] == 0)
			continue;
		if (vpe != 0)
			printk(", ");
		printk("VPE %d: TC", vpe);
		for (i = 0; i < tcpervpe[vpe]; i++) {
			if (tc != 0) {
				smtc_tc_setup(vpe, tc, cpu);
				cpu++;
			}
			printk(" %d", tc);
			tc++;
		}
		if (vpe != 0) {
			write_vpe_c0_vpeconf0(read_vpe_c0_vpeconf0() |
					      VPECONF0_MVP);

			write_vpe_c0_cause(0);

			write_vpe_c0_status((read_vpe_c0_status()
				& ~(ST0_BEV | ST0_ERL | ST0_EXL | ST0_IM))
				| (STATUSF_IP0 | STATUSF_IP1 | STATUSF_IP7
				| ST0_IE));
			write_vpe_c0_config(read_c0_config());
			
			write_vpe_c0_compare(0);
			
			write_vpe_c0_config7(read_c0_config7());
			write_vpe_c0_count(read_c0_count() + CP0_SKEW);
			ehb();
		}
		
		write_vpe_c0_vpecontrol(read_vpe_c0_vpecontrol() | VPECONTROL_TE);
		
		write_vpe_c0_vpeconf0(read_vpe_c0_vpeconf0() | VPECONF0_VPA);
	}

	while (tc < (((val & MVPCONF0_PTC) >> MVPCONF0_PTC_SHIFT) + 1)) {
		set_cpu_possible(tc, false);
		set_cpu_present(tc, false);
		tc++;
	}

	
	write_c0_mvpcontrol( read_c0_mvpcontrol() & ~ MVPCONTROL_VPC );

	printk("\n");

	

#ifdef CONFIG_MIPS_MT_FPAFF
	for (tc = 0; tc < ntc; tc++) {
		if (cpu_data[tc].options & MIPS_CPU_FPU)
			cpu_set(tc, mt_fpu_cpumask);
	}
#endif

	

	

	setup_cross_vpe_interrupts(nvpe);

	
	nipi = NR_CPUS * IPIBUF_PER_CPU;
	if (ipibuffers > 0)
		nipi = ipibuffers;

	pipi = kmalloc(nipi *sizeof(struct smtc_ipi), GFP_KERNEL);
	if (pipi == NULL)
		panic("kmalloc of IPI message buffers failed");
	else
		printk("IPI buffer pool of %d buffers\n", nipi);
	for (i = 0; i < nipi; i++) {
		smtc_ipi_nq(&freeIPIq, pipi);
		pipi++;
	}

	
	emt(EMT_ENABLE);
	evpe(EVPE_ENABLE);
	local_irq_restore(flags);
	
	init_smtc_stats();
}


void __cpuinit smtc_boot_secondary(int cpu, struct task_struct *idle)
{
	extern u32 kernelsp[NR_CPUS];
	unsigned long flags;
	int mtflags;

	LOCK_MT_PRA();
	if (cpu_data[cpu].vpe_id != cpu_data[smp_processor_id()].vpe_id) {
		dvpe();
	}
	settc(cpu_data[cpu].tc_id);

	
	write_tc_c0_tcrestart((unsigned long)&smp_bootstrap);

	
	kernelsp[cpu] = __KSTK_TOS(idle);
	write_tc_gpr_sp(__KSTK_TOS(idle));

	
	write_tc_gpr_gp((unsigned long)task_thread_info(idle));

	smtc_status |= SMTC_MTC_ACTIVE;
	write_tc_c0_tchalt(0);
	if (cpu_data[cpu].vpe_id != cpu_data[smp_processor_id()].vpe_id) {
		evpe(EVPE_ENABLE);
	}
	UNLOCK_MT_PRA();
}

void smtc_init_secondary(void)
{
	local_irq_enable();
}

void smtc_smp_finish(void)
{
	int cpu = smp_processor_id();

	if (cpu > 0 && (cpu_data[cpu].vpe_id != cpu_data[cpu - 1].vpe_id))
		write_c0_compare(read_c0_count() + mips_hpt_frequency/HZ);

	printk("TC %d going on-line as CPU %d\n",
		cpu_data[smp_processor_id()].tc_id, smp_processor_id());
}

void smtc_cpus_done(void)
{
}



int setup_irq_smtc(unsigned int irq, struct irqaction * new,
			unsigned long hwmask)
{
#ifdef CONFIG_SMTC_IDLE_HOOK_DEBUG
	unsigned int vpe = current_cpu_data.vpe_id;

	vpemask[vpe][irq - MIPS_CPU_IRQ_BASE] = 1;
#endif
	irq_hwmask[irq] = hwmask;

	return setup_irq(irq, new);
}

#ifdef CONFIG_MIPS_MT_SMTC_IRQAFF

void smtc_set_irq_affinity(unsigned int irq, cpumask_t affinity)
{
}

void smtc_forward_irq(struct irq_data *d)
{
	unsigned int irq = d->irq;
	int target;


	target = cpumask_first(d->affinity);


	
	if (target >= NR_CPUS)
		do_IRQ_no_affinity(irq);
	else
		smtc_send_ipi(target, IRQ_AFFINITY_IPI, irq);
}

#endif 


static void smtc_ipi_qdump(void)
{
	int i;
	struct smtc_ipi *temp;

	for (i = 0; i < NR_CPUS ;i++) {
		pr_info("IPIQ[%d]: head = 0x%x, tail = 0x%x, depth = %d\n",
			i, (unsigned)IPIQ[i].head, (unsigned)IPIQ[i].tail,
			IPIQ[i].depth);
		temp = IPIQ[i].head;

		while (temp != IPIQ[i].tail) {
			pr_debug("%d %d %d: ", temp->type, temp->dest,
			       (int)temp->arg);
#ifdef	SMTC_IPI_DEBUG
		    pr_debug("%u %lu\n", temp->sender, temp->stamp);
#else
		    pr_debug("\n");
#endif
		    temp = temp->flink;
		}
	}
}

static inline int atomic_postincrement(atomic_t *v)
{
	unsigned long result;

	unsigned long temp;

	__asm__ __volatile__(
	"1:	ll	%0, %2					\n"
	"	addu	%1, %0, 1				\n"
	"	sc	%1, %2					\n"
	"	beqz	%1, 1b					\n"
	__WEAK_LLSC_MB
	: "=&r" (result), "=&r" (temp), "=m" (v->counter)
	: "m" (v->counter)
	: "memory");

	return result;
}

void smtc_send_ipi(int cpu, int type, unsigned int action)
{
	int tcstatus;
	struct smtc_ipi *pipi;
	unsigned long flags;
	int mtflags;
	unsigned long tcrestart;
	extern void r4k_wait_irqoff(void), __pastwait(void);
	int set_resched_flag = (type == LINUX_SMP_IPI &&
				action == SMP_RESCHEDULE_YOURSELF);

	if (cpu == smp_processor_id()) {
		printk("Cannot Send IPI to self!\n");
		return;
	}
	if (set_resched_flag && IPIQ[cpu].resched_flag != 0)
		return; 

	
	pipi = smtc_ipi_dq(&freeIPIq);
	if (pipi == NULL) {
		bust_spinlocks(1);
		mips_mt_regdump(dvpe());
		panic("IPI Msg. Buffers Depleted");
	}
	pipi->type = type;
	pipi->arg = (void *)action;
	pipi->dest = cpu;
	if (cpu_data[cpu].vpe_id != cpu_data[smp_processor_id()].vpe_id) {
		
		IPIQ[cpu].resched_flag |= set_resched_flag;
		smtc_ipi_nq(&IPIQ[cpu], pipi);
		LOCK_CORE_PRA();
		settc(cpu_data[cpu].tc_id);
		write_vpe_c0_cause(read_vpe_c0_cause() | C_SW1);
		UNLOCK_CORE_PRA();
	} else {
		LOCK_CORE_PRA();
		settc(cpu_data[cpu].tc_id);
		
		write_tc_c0_tchalt(TCHALT_H);
		mips_ihb();

		tcstatus = read_tc_c0_tcstatus();

		if ((tcstatus & TCSTATUS_IXMT) != 0) {
			if (cpu_wait == r4k_wait_irqoff) {
				tcrestart = read_tc_c0_tcrestart();
				if (tcrestart >= (unsigned long)r4k_wait_irqoff
				    && tcrestart < (unsigned long)__pastwait) {
					write_tc_c0_tcrestart(__pastwait);
					tcstatus &= ~TCSTATUS_IXMT;
					write_tc_c0_tcstatus(tcstatus);
					goto postdirect;
				}
			}
			write_tc_c0_tchalt(0);
			UNLOCK_CORE_PRA();
			IPIQ[cpu].resched_flag |= set_resched_flag;
			smtc_ipi_nq(&IPIQ[cpu], pipi);
		} else {
postdirect:
			post_direct_ipi(cpu, pipi);
			write_tc_c0_tchalt(0);
			UNLOCK_CORE_PRA();
		}
	}
}

static void post_direct_ipi(int cpu, struct smtc_ipi *pipi)
{
	struct pt_regs *kstack;
	unsigned long tcstatus;
	unsigned long tcrestart;
	extern u32 kernelsp[NR_CPUS];
	extern void __smtc_ipi_vector(void);

	
	tcstatus = read_tc_c0_tcstatus();
	tcrestart = read_tc_c0_tcrestart();
	
	if ((tcrestart & 0x80000000)
	    && ((*(unsigned int *)tcrestart & 0xfe00003f) == 0x42000020)) {
		tcrestart += 4;
	}
	if (tcstatus & ST0_CU0)  {
		
		kstack = ((struct pt_regs *)read_tc_gpr_sp()) - 1;
	} else {
		kstack = ((struct pt_regs *)kernelsp[cpu]) - 1;
	}

	kstack->cp0_epc = (long)tcrestart;
	
	kstack->cp0_tcstatus = tcstatus;
	
	kstack->pad0[4] = (unsigned long)pipi;
	
	kstack->pad0[5] = (unsigned long)&ipi_decode;
	
	tcstatus |= TCSTATUS_IXMT;
	tcstatus &= ~TCSTATUS_TKSU;
	write_tc_c0_tcstatus(tcstatus);
	ehb();
	
	write_tc_c0_tcrestart(__smtc_ipi_vector);
}

static void ipi_resched_interrupt(void)
{
	scheduler_ipi();
}

static void ipi_call_interrupt(void)
{
	
	smp_call_function_interrupt();
}

DECLARE_PER_CPU(struct clock_event_device, mips_clockevent_device);

static void __irq_entry smtc_clock_tick_interrupt(void)
{
	unsigned int cpu = smp_processor_id();
	struct clock_event_device *cd;
	int irq = MIPS_CPU_IRQ_BASE + 1;

	irq_enter();
	kstat_incr_irqs_this_cpu(irq, irq_to_desc(irq));
	cd = &per_cpu(mips_clockevent_device, cpu);
	cd->event_handler(cd);
	irq_exit();
}

void ipi_decode(struct smtc_ipi *pipi)
{
	void *arg_copy = pipi->arg;
	int type_copy = pipi->type;

	smtc_ipi_nq(&freeIPIq, pipi);

	switch (type_copy) {
	case SMTC_CLOCK_TICK:
		smtc_clock_tick_interrupt();
		break;

	case LINUX_SMP_IPI:
		switch ((int)arg_copy) {
		case SMP_RESCHEDULE_YOURSELF:
			ipi_resched_interrupt();
			break;
		case SMP_CALL_FUNCTION:
			ipi_call_interrupt();
			break;
		default:
			printk("Impossible SMTC IPI Argument %p\n", arg_copy);
			break;
		}
		break;
#ifdef CONFIG_MIPS_MT_SMTC_IRQAFF
	case IRQ_AFFINITY_IPI:
		do_IRQ_no_affinity((int)arg_copy);
		break;
#endif 
	default:
		printk("Impossible SMTC IPI Type 0x%x\n", type_copy);
		break;
	}
}


void deferred_smtc_ipi(void)
{
	int cpu = smp_processor_id();


	while (IPIQ[cpu].head != NULL) {
		struct smtc_ipi_q *q = &IPIQ[cpu];
		struct smtc_ipi *pipi;
		unsigned long flags;

		local_irq_save(flags);
		spin_lock(&q->lock);
		pipi = __smtc_ipi_dq(q);
		spin_unlock(&q->lock);
		if (pipi != NULL) {
			if (pipi->type == LINUX_SMP_IPI &&
			    (int)pipi->arg == SMP_RESCHEDULE_YOURSELF)
				IPIQ[cpu].resched_flag = 0;
			ipi_decode(pipi);
		}
		__arch_local_irq_restore(flags);
	}
}


static int cpu_ipi_irq = MIPS_CPU_IRQ_BASE + MIPS_CPU_IPI_IRQ;

static irqreturn_t ipi_interrupt(int irq, void *dev_idm)
{
	int my_vpe = cpu_data[smp_processor_id()].vpe_id;
	int my_tc = cpu_data[smp_processor_id()].tc_id;
	int cpu;
	struct smtc_ipi *pipi;
	unsigned long tcstatus;
	int sent;
	unsigned long flags;
	unsigned int mtflags;
	unsigned int vpflags;

	local_irq_save(flags);
	vpflags = dvpe();
	clear_c0_cause(0x100 << MIPS_CPU_IPI_IRQ);
	set_c0_status(0x100 << MIPS_CPU_IPI_IRQ);
	irq_enable_hazard();
	evpe(vpflags);
	local_irq_restore(flags);


	for_each_online_cpu(cpu) {
		if (cpu_data[cpu].vpe_id != my_vpe)
			continue;

		pipi = smtc_ipi_dq(&IPIQ[cpu]);
		if (pipi != NULL) {
			if (cpu_data[cpu].tc_id != my_tc) {
				sent = 0;
				LOCK_MT_PRA();
				settc(cpu_data[cpu].tc_id);
				write_tc_c0_tchalt(TCHALT_H);
				mips_ihb();
				tcstatus = read_tc_c0_tcstatus();
				if ((tcstatus & TCSTATUS_IXMT) == 0) {
					post_direct_ipi(cpu, pipi);
					sent = 1;
				}
				write_tc_c0_tchalt(0);
				UNLOCK_MT_PRA();
				if (!sent) {
					smtc_ipi_req(&IPIQ[cpu], pipi);
				}
			} else {
				local_irq_save(flags);
				if (pipi->type == LINUX_SMP_IPI &&
				    (int)pipi->arg == SMP_RESCHEDULE_YOURSELF)
					IPIQ[cpu].resched_flag = 0;
				ipi_decode(pipi);
				local_irq_restore(flags);
			}
		}
	}

	return IRQ_HANDLED;
}

static void ipi_irq_dispatch(void)
{
	do_IRQ(cpu_ipi_irq);
}

static struct irqaction irq_ipi = {
	.handler	= ipi_interrupt,
	.flags		= IRQF_PERCPU,
	.name		= "SMTC_IPI"
};

static void setup_cross_vpe_interrupts(unsigned int nvpe)
{
	if (nvpe < 1)
		return;

	if (!cpu_has_vint)
		panic("SMTC Kernel requires Vectored Interrupt support");

	set_vi_handler(MIPS_CPU_IPI_IRQ, ipi_irq_dispatch);

	setup_irq_smtc(cpu_ipi_irq, &irq_ipi, (0x100 << MIPS_CPU_IPI_IRQ));

	irq_set_handler(cpu_ipi_irq, handle_percpu_irq);
}



void smtc_ipi_replay(void)
{
	unsigned int cpu = smp_processor_id();

	while (IPIQ[cpu].head != NULL) {
		struct smtc_ipi_q *q = &IPIQ[cpu];
		struct smtc_ipi *pipi;
		unsigned long flags;

		local_irq_save(flags);

		spin_lock(&q->lock);
		pipi = __smtc_ipi_dq(q);
		spin_unlock(&q->lock);
		__arch_local_irq_restore(flags);

		if (pipi) {
			self_ipi(pipi);
			smtc_cpu_stats[cpu].selfipis++;
		}
	}
}

EXPORT_SYMBOL(smtc_ipi_replay);

void smtc_idle_loop_hook(void)
{
#ifdef CONFIG_SMTC_IDLE_HOOK_DEBUG
	int im;
	int flags;
	int mtflags;
	int bit;
	int vpe;
	int tc;
	int hook_ntcs;
	char *pdb_msg;
	char id_ho_db_msg[768]; 

	if (atomic_read(&idle_hook_initialized) == 0) { 
		if (atomic_add_return(1, &idle_hook_initialized) == 1) {
			int mvpconf0;
			
			mvpconf0 = read_c0_mvpconf0();
			hook_ntcs = ((mvpconf0 & MVPCONF0_PTC) >> MVPCONF0_PTC_SHIFT) + 1;
			if (hook_ntcs > NR_CPUS)
				hook_ntcs = NR_CPUS;
			for (tc = 0; tc < hook_ntcs; tc++) {
				tcnoprog[tc] = 0;
				clock_hang_reported[tc] = 0;
	    		}
			for (vpe = 0; vpe < 2; vpe++)
				for (im = 0; im < 8; im++)
					imstuckcount[vpe][im] = 0;
			printk("Idle loop test hook initialized for %d TCs\n", hook_ntcs);
			atomic_set(&idle_hook_initialized, 1000);
		} else {
			
			while (atomic_read(&idle_hook_initialized) < 1000)
				;
		}
	}

	
	if (read_c0_tcstatus() & 0x400) {
		write_c0_tcstatus(read_c0_tcstatus() & ~0x400);
		ehb();
		printk("Dangling IXMT in cpu_idle()\n");
	}

	
#define IM_LIMIT 2000
	local_irq_save(flags);
	mtflags = dmt();
	pdb_msg = &id_ho_db_msg[0];
	im = read_c0_status();
	vpe = current_cpu_data.vpe_id;
	for (bit = 0; bit < 8; bit++) {
		if (vpemask[vpe][bit]) {
			if (!(im & (0x100 << bit)))
				imstuckcount[vpe][bit]++;
			else
				imstuckcount[vpe][bit] = 0;
			if (imstuckcount[vpe][bit] > IM_LIMIT) {
				set_c0_status(0x100 << bit);
				ehb();
				imstuckcount[vpe][bit] = 0;
				pdb_msg += sprintf(pdb_msg,
					"Dangling IM %d fixed for VPE %d\n", bit,
					vpe);
			}
		}
	}

	emt(mtflags);
	local_irq_restore(flags);
	if (pdb_msg != &id_ho_db_msg[0])
		printk("CPU%d: %s", smp_processor_id(), id_ho_db_msg);
#endif 

	smtc_ipi_replay();
}

void smtc_soft_dump(void)
{
	int i;

	printk("Counter Interrupts taken per CPU (TC)\n");
	for (i=0; i < NR_CPUS; i++) {
		printk("%d: %ld\n", i, smtc_cpu_stats[i].timerints);
	}
	printk("Self-IPI invocations:\n");
	for (i=0; i < NR_CPUS; i++) {
		printk("%d: %ld\n", i, smtc_cpu_stats[i].selfipis);
	}
	smtc_ipi_qdump();
	printk("%d Recoveries of \"stolen\" FPU\n",
	       atomic_read(&smtc_fpu_recoveries));
}



void smtc_get_new_mmu_context(struct mm_struct *mm, unsigned long cpu)
{
	unsigned long flags, mtflags, tcstat, prevhalt, asid;
	int tlb, i;


	local_irq_save(flags);
	if (smtc_status & SMTC_TLB_SHARED) {
		mtflags = dvpe();
		tlb = 0;
	} else {
		mtflags = dmt();
		tlb = cpu_data[cpu].vpe_id;
	}
	asid = asid_cache(cpu);

	do {
		if (!((asid += ASID_INC) & ASID_MASK) ) {
			if (cpu_has_vtag_icache)
				flush_icache_all();
			
			for_each_online_cpu(i) {
				if ((i != smp_processor_id()) &&
				    ((smtc_status & SMTC_TLB_SHARED) ||
				     (cpu_data[i].vpe_id == cpu_data[cpu].vpe_id))) {
					settc(cpu_data[i].tc_id);
					prevhalt = read_tc_c0_tchalt() & TCHALT_H;
					if (!prevhalt) {
						write_tc_c0_tchalt(TCHALT_H);
						mips_ihb();
					}
					tcstat = read_tc_c0_tcstatus();
					smtc_live_asid[tlb][(tcstat & ASID_MASK)] |= (asiduse)(0x1 << i);
					if (!prevhalt)
						write_tc_c0_tchalt(0);
				}
			}
			if (!asid)		
				asid = ASID_FIRST_VERSION;
			local_flush_tlb_all();	
		}
	} while (smtc_live_asid[tlb][(asid & ASID_MASK)]);

	for_each_online_cpu(i) {
		if ((smtc_status & SMTC_TLB_SHARED) ||
		    (cpu_data[i].vpe_id == cpu_data[cpu].vpe_id))
			cpu_context(i, mm) = asid_cache(i) = asid;
	}

	if (smtc_status & SMTC_TLB_SHARED)
		evpe(mtflags);
	else
		emt(mtflags);
	local_irq_restore(flags);
}


void smtc_flush_tlb_asid(unsigned long asid)
{
	int entry;
	unsigned long ehi;

	entry = read_c0_wired();

	
	while (entry < current_cpu_data.tlbsize) {
		write_c0_index(entry);
		ehb();
		tlb_read();
		ehb();
		ehi = read_c0_entryhi();
		if ((ehi & ASID_MASK) == asid) {
		    write_c0_entryhi(CKSEG0 + (entry << (PAGE_SHIFT + 1)));
		    write_c0_entrylo0(0);
		    write_c0_entrylo1(0);
		    mtc0_tlbw_hazard();
		    tlb_write_indexed();
		}
		entry++;
	}
	write_c0_index(PARKED_INDEX);
	tlbw_use_hazard();
}


static int halt_state_save[NR_CPUS];


void smtc_cflush_lockdown(void)
{
	int cpu;

	for_each_online_cpu(cpu) {
		if (cpu != smp_processor_id()) {
			settc(cpu_data[cpu].tc_id);
			halt_state_save[cpu] = read_tc_c0_tchalt();
			write_tc_c0_tchalt(TCHALT_H);
		}
	}
	mips_ihb();
}


void smtc_cflush_release(void)
{
	int cpu;

	mips_ihb();

	for_each_online_cpu(cpu) {
		if (cpu != smp_processor_id()) {
			settc(cpu_data[cpu].tc_id);
			write_tc_c0_tchalt(halt_state_save[cpu]);
		}
	}
	mips_ihb();
}
