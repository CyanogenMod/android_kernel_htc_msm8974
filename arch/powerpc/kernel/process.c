/*
 *  Derived from "arch/i386/kernel/process.c"
 *    Copyright (C) 1995  Linus Torvalds
 *
 *  Updated and modified by Cort Dougan (cort@cs.nmt.edu) and
 *  Paul Mackerras (paulus@cs.anu.edu.au)
 *
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/elf.h>
#include <linux/init.h>
#include <linux/prctl.h>
#include <linux/init_task.h>
#include <linux/export.h>
#include <linux/kallsyms.h>
#include <linux/mqueue.h>
#include <linux/hardirq.h>
#include <linux/utsname.h>
#include <linux/ftrace.h>
#include <linux/kernel_stat.h>
#include <linux/personality.h>
#include <linux/random.h>
#include <linux/hw_breakpoint.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/time.h>
#include <asm/runlatch.h>
#include <asm/syscalls.h>
#include <asm/switch_to.h>
#include <asm/debug.h>
#ifdef CONFIG_PPC64
#include <asm/firmware.h>
#endif
#include <linux/kprobes.h>
#include <linux/kdebug.h>

extern unsigned long _get_SP(void);

#ifndef CONFIG_SMP
struct task_struct *last_task_used_math = NULL;
struct task_struct *last_task_used_altivec = NULL;
struct task_struct *last_task_used_vsx = NULL;
struct task_struct *last_task_used_spe = NULL;
#endif

void flush_fp_to_thread(struct task_struct *tsk)
{
	if (tsk->thread.regs) {
		preempt_disable();
		if (tsk->thread.regs->msr & MSR_FP) {
#ifdef CONFIG_SMP
			BUG_ON(tsk != current);
#endif
			giveup_fpu(tsk);
		}
		preempt_enable();
	}
}
EXPORT_SYMBOL_GPL(flush_fp_to_thread);

void enable_kernel_fp(void)
{
	WARN_ON(preemptible());

#ifdef CONFIG_SMP
	if (current->thread.regs && (current->thread.regs->msr & MSR_FP))
		giveup_fpu(current);
	else
		giveup_fpu(NULL);	
#else
	giveup_fpu(last_task_used_math);
#endif 
}
EXPORT_SYMBOL(enable_kernel_fp);

#ifdef CONFIG_ALTIVEC
void enable_kernel_altivec(void)
{
	WARN_ON(preemptible());

#ifdef CONFIG_SMP
	if (current->thread.regs && (current->thread.regs->msr & MSR_VEC))
		giveup_altivec(current);
	else
		giveup_altivec(NULL);	
#else
	giveup_altivec(last_task_used_altivec);
#endif 
}
EXPORT_SYMBOL(enable_kernel_altivec);

void flush_altivec_to_thread(struct task_struct *tsk)
{
	if (tsk->thread.regs) {
		preempt_disable();
		if (tsk->thread.regs->msr & MSR_VEC) {
#ifdef CONFIG_SMP
			BUG_ON(tsk != current);
#endif
			giveup_altivec(tsk);
		}
		preempt_enable();
	}
}
EXPORT_SYMBOL_GPL(flush_altivec_to_thread);
#endif 

#ifdef CONFIG_VSX
#if 0
void enable_kernel_vsx(void)
{
	WARN_ON(preemptible());

#ifdef CONFIG_SMP
	if (current->thread.regs && (current->thread.regs->msr & MSR_VSX))
		giveup_vsx(current);
	else
		giveup_vsx(NULL);	
#else
	giveup_vsx(last_task_used_vsx);
#endif 
}
EXPORT_SYMBOL(enable_kernel_vsx);
#endif

void giveup_vsx(struct task_struct *tsk)
{
	giveup_fpu(tsk);
	giveup_altivec(tsk);
	__giveup_vsx(tsk);
}

void flush_vsx_to_thread(struct task_struct *tsk)
{
	if (tsk->thread.regs) {
		preempt_disable();
		if (tsk->thread.regs->msr & MSR_VSX) {
#ifdef CONFIG_SMP
			BUG_ON(tsk != current);
#endif
			giveup_vsx(tsk);
		}
		preempt_enable();
	}
}
EXPORT_SYMBOL_GPL(flush_vsx_to_thread);
#endif 

#ifdef CONFIG_SPE

void enable_kernel_spe(void)
{
	WARN_ON(preemptible());

#ifdef CONFIG_SMP
	if (current->thread.regs && (current->thread.regs->msr & MSR_SPE))
		giveup_spe(current);
	else
		giveup_spe(NULL);	
#else
	giveup_spe(last_task_used_spe);
#endif 
}
EXPORT_SYMBOL(enable_kernel_spe);

void flush_spe_to_thread(struct task_struct *tsk)
{
	if (tsk->thread.regs) {
		preempt_disable();
		if (tsk->thread.regs->msr & MSR_SPE) {
#ifdef CONFIG_SMP
			BUG_ON(tsk != current);
#endif
			tsk->thread.spefscr = mfspr(SPRN_SPEFSCR);
			giveup_spe(tsk);
		}
		preempt_enable();
	}
}
#endif 

#ifndef CONFIG_SMP
void discard_lazy_cpu_state(void)
{
	preempt_disable();
	if (last_task_used_math == current)
		last_task_used_math = NULL;
#ifdef CONFIG_ALTIVEC
	if (last_task_used_altivec == current)
		last_task_used_altivec = NULL;
#endif 
#ifdef CONFIG_VSX
	if (last_task_used_vsx == current)
		last_task_used_vsx = NULL;
#endif 
#ifdef CONFIG_SPE
	if (last_task_used_spe == current)
		last_task_used_spe = NULL;
#endif
	preempt_enable();
}
#endif 

#ifdef CONFIG_PPC_ADV_DEBUG_REGS
void do_send_trap(struct pt_regs *regs, unsigned long address,
		  unsigned long error_code, int signal_code, int breakpt)
{
	siginfo_t info;

	if (notify_die(DIE_DABR_MATCH, "dabr_match", regs, error_code,
			11, SIGSEGV) == NOTIFY_STOP)
		return;

	
	info.si_signo = SIGTRAP;
	info.si_errno = breakpt;	
	info.si_code = signal_code;
	info.si_addr = (void __user *)address;
	force_sig_info(SIGTRAP, &info, current);
}
#else	
void do_dabr(struct pt_regs *regs, unsigned long address,
		    unsigned long error_code)
{
	siginfo_t info;

	if (notify_die(DIE_DABR_MATCH, "dabr_match", regs, error_code,
			11, SIGSEGV) == NOTIFY_STOP)
		return;

	if (debugger_dabr_match(regs))
		return;

	
	set_dabr(0);

	
	info.si_signo = SIGTRAP;
	info.si_errno = 0;
	info.si_code = TRAP_HWBKPT;
	info.si_addr = (void __user *)address;
	force_sig_info(SIGTRAP, &info, current);
}
#endif	

static DEFINE_PER_CPU(unsigned long, current_dabr);

#ifdef CONFIG_PPC_ADV_DEBUG_REGS
static void set_debug_reg_defaults(struct thread_struct *thread)
{
	thread->iac1 = thread->iac2 = 0;
#if CONFIG_PPC_ADV_DEBUG_IACS > 2
	thread->iac3 = thread->iac4 = 0;
#endif
	thread->dac1 = thread->dac2 = 0;
#if CONFIG_PPC_ADV_DEBUG_DVCS > 0
	thread->dvc1 = thread->dvc2 = 0;
#endif
	thread->dbcr0 = 0;
#ifdef CONFIG_BOOKE
	thread->dbcr1 = DBCR1_IAC1US | DBCR1_IAC2US |	\
			DBCR1_IAC3US | DBCR1_IAC4US;
	thread->dbcr2 = DBCR2_DAC1US | DBCR2_DAC2US;
#else
	thread->dbcr1 = 0;
#endif
}

static void prime_debug_regs(struct thread_struct *thread)
{
	mtspr(SPRN_IAC1, thread->iac1);
	mtspr(SPRN_IAC2, thread->iac2);
#if CONFIG_PPC_ADV_DEBUG_IACS > 2
	mtspr(SPRN_IAC3, thread->iac3);
	mtspr(SPRN_IAC4, thread->iac4);
#endif
	mtspr(SPRN_DAC1, thread->dac1);
	mtspr(SPRN_DAC2, thread->dac2);
#if CONFIG_PPC_ADV_DEBUG_DVCS > 0
	mtspr(SPRN_DVC1, thread->dvc1);
	mtspr(SPRN_DVC2, thread->dvc2);
#endif
	mtspr(SPRN_DBCR0, thread->dbcr0);
	mtspr(SPRN_DBCR1, thread->dbcr1);
#ifdef CONFIG_BOOKE
	mtspr(SPRN_DBCR2, thread->dbcr2);
#endif
}
static void switch_booke_debug_regs(struct thread_struct *new_thread)
{
	if ((current->thread.dbcr0 & DBCR0_IDM)
		|| (new_thread->dbcr0 & DBCR0_IDM))
			prime_debug_regs(new_thread);
}
#else	
#ifndef CONFIG_HAVE_HW_BREAKPOINT
static void set_debug_reg_defaults(struct thread_struct *thread)
{
	if (thread->dabr) {
		thread->dabr = 0;
		set_dabr(0);
	}
}
#endif 
#endif	

int set_dabr(unsigned long dabr)
{
	__get_cpu_var(current_dabr) = dabr;

	if (ppc_md.set_dabr)
		return ppc_md.set_dabr(dabr);

	
#ifdef CONFIG_PPC_ADV_DEBUG_REGS
	mtspr(SPRN_DAC1, dabr);
#ifdef CONFIG_PPC_47x
	isync();
#endif
#elif defined(CONFIG_PPC_BOOK3S)
	mtspr(SPRN_DABR, dabr);
#endif


	return 0;
}

#ifdef CONFIG_PPC64
DEFINE_PER_CPU(struct cpu_usage, cpu_usage_array);
#endif

struct task_struct *__switch_to(struct task_struct *prev,
	struct task_struct *new)
{
	struct thread_struct *new_thread, *old_thread;
	unsigned long flags;
	struct task_struct *last;
#ifdef CONFIG_PPC_BOOK3S_64
	struct ppc64_tlb_batch *batch;
#endif

#ifdef CONFIG_SMP
	if (prev->thread.regs && (prev->thread.regs->msr & MSR_FP))
		giveup_fpu(prev);
#ifdef CONFIG_ALTIVEC
	if (prev->thread.regs && (prev->thread.regs->msr & MSR_VEC))
		giveup_altivec(prev);
#endif 
#ifdef CONFIG_VSX
	if (prev->thread.regs && (prev->thread.regs->msr & MSR_VSX))
		
		__giveup_vsx(prev);
#endif 
#ifdef CONFIG_SPE
	if ((prev->thread.regs && (prev->thread.regs->msr & MSR_SPE)))
		giveup_spe(prev);
#endif 

#else  
#ifdef CONFIG_ALTIVEC
	if (new->thread.regs && last_task_used_altivec == new)
		new->thread.regs->msr |= MSR_VEC;
#endif 
#ifdef CONFIG_VSX
	if (new->thread.regs && last_task_used_vsx == new)
		new->thread.regs->msr |= MSR_VSX;
#endif 
#ifdef CONFIG_SPE
	if (new->thread.regs && last_task_used_spe == new)
		new->thread.regs->msr |= MSR_SPE;
#endif 

#endif 

#ifdef CONFIG_PPC_ADV_DEBUG_REGS
	switch_booke_debug_regs(&new->thread);
#else
#ifndef CONFIG_HAVE_HW_BREAKPOINT
	if (unlikely(__get_cpu_var(current_dabr) != new->thread.dabr))
		set_dabr(new->thread.dabr);
#endif 
#endif


	new_thread = &new->thread;
	old_thread = &current->thread;

#ifdef CONFIG_PPC64
	if (firmware_has_feature(FW_FEATURE_SPLPAR)) {
		struct cpu_usage *cu = &__get_cpu_var(cpu_usage_array);
		long unsigned start_tb, current_tb;
		start_tb = old_thread->start_tb;
		cu->current_tb = current_tb = mfspr(SPRN_PURR);
		old_thread->accum_tb += (current_tb - start_tb);
		new_thread->start_tb = current_tb;
	}
#endif 

#ifdef CONFIG_PPC_BOOK3S_64
	batch = &__get_cpu_var(ppc64_tlb_batch);
	if (batch->active) {
		current_thread_info()->local_flags |= _TLF_LAZY_MMU;
		if (batch->index)
			__flush_tlb_pending(batch);
		batch->active = 0;
	}
#endif 

	local_irq_save(flags);

	account_system_vtime(current);
	account_process_vtime(current);

	hard_irq_disable();
	last = _switch(old_thread, new_thread);

#ifdef CONFIG_PPC_BOOK3S_64
	if (current_thread_info()->local_flags & _TLF_LAZY_MMU) {
		current_thread_info()->local_flags &= ~_TLF_LAZY_MMU;
		batch = &__get_cpu_var(ppc64_tlb_batch);
		batch->active = 1;
	}
#endif 

	local_irq_restore(flags);

	return last;
}

static int instructions_to_print = 16;

static void show_instructions(struct pt_regs *regs)
{
	int i;
	unsigned long pc = regs->nip - (instructions_to_print * 3 / 4 *
			sizeof(int));

	printk("Instruction dump:");

	for (i = 0; i < instructions_to_print; i++) {
		int instr;

		if (!(i % 8))
			printk("\n");

#if !defined(CONFIG_BOOKE)
		if (!(regs->msr & MSR_IR))
			pc = (unsigned long)phys_to_virt(pc);
#endif

		if (!__kernel_text_address(pc) ||
		     __get_user(instr, (unsigned int __user *)pc)) {
			printk(KERN_CONT "XXXXXXXX ");
		} else {
			if (regs->nip == pc)
				printk(KERN_CONT "<%08x> ", instr);
			else
				printk(KERN_CONT "%08x ", instr);
		}

		pc += sizeof(int);
	}

	printk("\n");
}

static struct regbit {
	unsigned long bit;
	const char *name;
} msr_bits[] = {
#if defined(CONFIG_PPC64) && !defined(CONFIG_BOOKE)
	{MSR_SF,	"SF"},
	{MSR_HV,	"HV"},
#endif
	{MSR_VEC,	"VEC"},
	{MSR_VSX,	"VSX"},
#ifdef CONFIG_BOOKE
	{MSR_CE,	"CE"},
#endif
	{MSR_EE,	"EE"},
	{MSR_PR,	"PR"},
	{MSR_FP,	"FP"},
	{MSR_ME,	"ME"},
#ifdef CONFIG_BOOKE
	{MSR_DE,	"DE"},
#else
	{MSR_SE,	"SE"},
	{MSR_BE,	"BE"},
#endif
	{MSR_IR,	"IR"},
	{MSR_DR,	"DR"},
	{MSR_PMM,	"PMM"},
#ifndef CONFIG_BOOKE
	{MSR_RI,	"RI"},
	{MSR_LE,	"LE"},
#endif
	{0,		NULL}
};

static void printbits(unsigned long val, struct regbit *bits)
{
	const char *sep = "";

	printk("<");
	for (; bits->bit; ++bits)
		if (val & bits->bit) {
			printk("%s%s", sep, bits->name);
			sep = ",";
		}
	printk(">");
}

#ifdef CONFIG_PPC64
#define REG		"%016lx"
#define REGS_PER_LINE	4
#define LAST_VOLATILE	13
#else
#define REG		"%08lx"
#define REGS_PER_LINE	8
#define LAST_VOLATILE	12
#endif

void show_regs(struct pt_regs * regs)
{
	int i, trap;

	printk("NIP: "REG" LR: "REG" CTR: "REG"\n",
	       regs->nip, regs->link, regs->ctr);
	printk("REGS: %p TRAP: %04lx   %s  (%s)\n",
	       regs, regs->trap, print_tainted(), init_utsname()->release);
	printk("MSR: "REG" ", regs->msr);
	printbits(regs->msr, msr_bits);
	printk("  CR: %08lx  XER: %08lx\n", regs->ccr, regs->xer);
#ifdef CONFIG_PPC64
	printk("SOFTE: %ld\n", regs->softe);
#endif
	trap = TRAP(regs);
	if ((regs->trap != 0xc00) && cpu_has_feature(CPU_FTR_CFAR))
		printk("CFAR: "REG"\n", regs->orig_gpr3);
	if (trap == 0x300 || trap == 0x600)
#if defined(CONFIG_4xx) || defined(CONFIG_BOOKE)
		printk("DEAR: "REG", ESR: "REG"\n", regs->dar, regs->dsisr);
#else
		printk("DAR: "REG", DSISR: %08lx\n", regs->dar, regs->dsisr);
#endif
	printk("TASK = %p[%d] '%s' THREAD: %p",
	       current, task_pid_nr(current), current->comm, task_thread_info(current));

#ifdef CONFIG_SMP
	printk(" CPU: %d", raw_smp_processor_id());
#endif 

	for (i = 0;  i < 32;  i++) {
		if ((i % REGS_PER_LINE) == 0)
			printk("\nGPR%02d: ", i);
		printk(REG " ", regs->gpr[i]);
		if (i == LAST_VOLATILE && !FULL_REGS(regs))
			break;
	}
	printk("\n");
#ifdef CONFIG_KALLSYMS
	printk("NIP ["REG"] %pS\n", regs->nip, (void *)regs->nip);
	printk("LR ["REG"] %pS\n", regs->link, (void *)regs->link);
#endif
	show_stack(current, (unsigned long *) regs->gpr[1]);
	if (!user_mode(regs))
		show_instructions(regs);
}

void exit_thread(void)
{
	discard_lazy_cpu_state();
}

void flush_thread(void)
{
	discard_lazy_cpu_state();

#ifdef CONFIG_HAVE_HW_BREAKPOINT
	flush_ptrace_hw_breakpoint(current);
#else 
	set_debug_reg_defaults(&current->thread);
#endif 
}

void
release_thread(struct task_struct *t)
{
}

void prepare_to_copy(struct task_struct *tsk)
{
	flush_fp_to_thread(current);
	flush_altivec_to_thread(current);
	flush_vsx_to_thread(current);
	flush_spe_to_thread(current);
#ifdef CONFIG_HAVE_HW_BREAKPOINT
	flush_ptrace_hw_breakpoint(tsk);
#endif 
}

extern unsigned long dscr_default; 

int copy_thread(unsigned long clone_flags, unsigned long usp,
		unsigned long unused, struct task_struct *p,
		struct pt_regs *regs)
{
	struct pt_regs *childregs, *kregs;
	extern void ret_from_fork(void);
	unsigned long sp = (unsigned long)task_stack_page(p) + THREAD_SIZE;

	CHECK_FULL_REGS(regs);
	
	sp -= sizeof(struct pt_regs);
	childregs = (struct pt_regs *) sp;
	*childregs = *regs;
	if ((childregs->msr & MSR_PR) == 0) {
		
		childregs->gpr[1] = sp + sizeof(struct pt_regs);
#ifdef CONFIG_PPC32
		childregs->gpr[2] = (unsigned long) p;
#else
		clear_tsk_thread_flag(p, TIF_32BIT);
#endif
		p->thread.regs = NULL;	
	} else {
		childregs->gpr[1] = usp;
		p->thread.regs = childregs;
		if (clone_flags & CLONE_SETTLS) {
#ifdef CONFIG_PPC64
			if (!is_32bit_task())
				childregs->gpr[13] = childregs->gpr[6];
			else
#endif
				childregs->gpr[2] = childregs->gpr[6];
		}
	}
	childregs->gpr[3] = 0;  
	sp -= STACK_FRAME_OVERHEAD;

	sp -= sizeof(struct pt_regs);
	kregs = (struct pt_regs *) sp;
	sp -= STACK_FRAME_OVERHEAD;
	p->thread.ksp = sp;
	p->thread.ksp_limit = (unsigned long)task_stack_page(p) +
				_ALIGN_UP(sizeof(struct thread_info), 16);

#ifdef CONFIG_PPC_STD_MMU_64
	if (mmu_has_feature(MMU_FTR_SLB)) {
		unsigned long sp_vsid;
		unsigned long llp = mmu_psize_defs[mmu_linear_psize].sllp;

		if (mmu_has_feature(MMU_FTR_1T_SEGMENT))
			sp_vsid = get_kernel_vsid(sp, MMU_SEGSIZE_1T)
				<< SLB_VSID_SHIFT_1T;
		else
			sp_vsid = get_kernel_vsid(sp, MMU_SEGSIZE_256M)
				<< SLB_VSID_SHIFT;
		sp_vsid |= SLB_VSID_KERNEL | llp;
		p->thread.ksp_vsid = sp_vsid;
	}
#endif 
#ifdef CONFIG_PPC64 
	if (cpu_has_feature(CPU_FTR_DSCR)) {
		if (current->thread.dscr_inherit) {
			p->thread.dscr_inherit = 1;
			p->thread.dscr = current->thread.dscr;
		} else if (0 != dscr_default) {
			p->thread.dscr_inherit = 1;
			p->thread.dscr = dscr_default;
		} else {
			p->thread.dscr_inherit = 0;
			p->thread.dscr = 0;
		}
	}
#endif

#ifdef CONFIG_PPC64
	kregs->nip = *((unsigned long *)ret_from_fork);
#else
	kregs->nip = (unsigned long)ret_from_fork;
#endif

	return 0;
}

void start_thread(struct pt_regs *regs, unsigned long start, unsigned long sp)
{
#ifdef CONFIG_PPC64
	unsigned long load_addr = regs->gpr[2];	
#endif

	if (!current->thread.regs) {
		struct pt_regs *regs = task_stack_page(current) + THREAD_SIZE;
		current->thread.regs = regs - 1;
	}

	memset(regs->gpr, 0, sizeof(regs->gpr));
	regs->ctr = 0;
	regs->link = 0;
	regs->xer = 0;
	regs->ccr = 0;
	regs->gpr[1] = sp;

	regs->trap &= ~1UL;

#ifdef CONFIG_PPC32
	regs->mq = 0;
	regs->nip = start;
	regs->msr = MSR_USER;
#else
	if (!is_32bit_task()) {
		unsigned long entry, toc;

		__get_user(entry, (unsigned long __user *)start);
		__get_user(toc, (unsigned long __user *)start+1);

		if (load_addr != 0) {
			entry += load_addr;
			toc   += load_addr;
		}
		regs->nip = entry;
		regs->gpr[2] = toc;
		regs->msr = MSR_USER64;
	} else {
		regs->nip = start;
		regs->gpr[2] = 0;
		regs->msr = MSR_USER32;
	}
#endif

	discard_lazy_cpu_state();
#ifdef CONFIG_VSX
	current->thread.used_vsr = 0;
#endif
	memset(current->thread.fpr, 0, sizeof(current->thread.fpr));
	current->thread.fpscr.val = 0;
#ifdef CONFIG_ALTIVEC
	memset(current->thread.vr, 0, sizeof(current->thread.vr));
	memset(&current->thread.vscr, 0, sizeof(current->thread.vscr));
	current->thread.vscr.u[3] = 0x00010000; 
	current->thread.vrsave = 0;
	current->thread.used_vr = 0;
#endif 
#ifdef CONFIG_SPE
	memset(current->thread.evr, 0, sizeof(current->thread.evr));
	current->thread.acc = 0;
	current->thread.spefscr = 0;
	current->thread.used_spe = 0;
#endif 
}

#define PR_FP_ALL_EXCEPT (PR_FP_EXC_DIV | PR_FP_EXC_OVF | PR_FP_EXC_UND \
		| PR_FP_EXC_RES | PR_FP_EXC_INV)

int set_fpexc_mode(struct task_struct *tsk, unsigned int val)
{
	struct pt_regs *regs = tsk->thread.regs;

	if (val & PR_FP_EXC_SW_ENABLE) {
#ifdef CONFIG_SPE
		if (cpu_has_feature(CPU_FTR_SPE)) {
			tsk->thread.fpexc_mode = val &
				(PR_FP_EXC_SW_ENABLE | PR_FP_ALL_EXCEPT);
			return 0;
		} else {
			return -EINVAL;
		}
#else
		return -EINVAL;
#endif
	}

	if (val > PR_FP_EXC_PRECISE)
		return -EINVAL;
	tsk->thread.fpexc_mode = __pack_fe01(val);
	if (regs != NULL && (regs->msr & MSR_FP) != 0)
		regs->msr = (regs->msr & ~(MSR_FE0|MSR_FE1))
			| tsk->thread.fpexc_mode;
	return 0;
}

int get_fpexc_mode(struct task_struct *tsk, unsigned long adr)
{
	unsigned int val;

	if (tsk->thread.fpexc_mode & PR_FP_EXC_SW_ENABLE)
#ifdef CONFIG_SPE
		if (cpu_has_feature(CPU_FTR_SPE))
			val = tsk->thread.fpexc_mode;
		else
			return -EINVAL;
#else
		return -EINVAL;
#endif
	else
		val = __unpack_fe01(tsk->thread.fpexc_mode);
	return put_user(val, (unsigned int __user *) adr);
}

int set_endian(struct task_struct *tsk, unsigned int val)
{
	struct pt_regs *regs = tsk->thread.regs;

	if ((val == PR_ENDIAN_LITTLE && !cpu_has_feature(CPU_FTR_REAL_LE)) ||
	    (val == PR_ENDIAN_PPC_LITTLE && !cpu_has_feature(CPU_FTR_PPC_LE)))
		return -EINVAL;

	if (regs == NULL)
		return -EINVAL;

	if (val == PR_ENDIAN_BIG)
		regs->msr &= ~MSR_LE;
	else if (val == PR_ENDIAN_LITTLE || val == PR_ENDIAN_PPC_LITTLE)
		regs->msr |= MSR_LE;
	else
		return -EINVAL;

	return 0;
}

int get_endian(struct task_struct *tsk, unsigned long adr)
{
	struct pt_regs *regs = tsk->thread.regs;
	unsigned int val;

	if (!cpu_has_feature(CPU_FTR_PPC_LE) &&
	    !cpu_has_feature(CPU_FTR_REAL_LE))
		return -EINVAL;

	if (regs == NULL)
		return -EINVAL;

	if (regs->msr & MSR_LE) {
		if (cpu_has_feature(CPU_FTR_REAL_LE))
			val = PR_ENDIAN_LITTLE;
		else
			val = PR_ENDIAN_PPC_LITTLE;
	} else
		val = PR_ENDIAN_BIG;

	return put_user(val, (unsigned int __user *)adr);
}

int set_unalign_ctl(struct task_struct *tsk, unsigned int val)
{
	tsk->thread.align_ctl = val;
	return 0;
}

int get_unalign_ctl(struct task_struct *tsk, unsigned long adr)
{
	return put_user(tsk->thread.align_ctl, (unsigned int __user *)adr);
}

#define TRUNC_PTR(x)	((typeof(x))(((unsigned long)(x)) & 0xffffffff))

int sys_clone(unsigned long clone_flags, unsigned long usp,
	      int __user *parent_tidp, void __user *child_threadptr,
	      int __user *child_tidp, int p6,
	      struct pt_regs *regs)
{
	CHECK_FULL_REGS(regs);
	if (usp == 0)
		usp = regs->gpr[1];	
#ifdef CONFIG_PPC64
	if (is_32bit_task()) {
		parent_tidp = TRUNC_PTR(parent_tidp);
		child_tidp = TRUNC_PTR(child_tidp);
	}
#endif
 	return do_fork(clone_flags, usp, regs, 0, parent_tidp, child_tidp);
}

int sys_fork(unsigned long p1, unsigned long p2, unsigned long p3,
	     unsigned long p4, unsigned long p5, unsigned long p6,
	     struct pt_regs *regs)
{
	CHECK_FULL_REGS(regs);
	return do_fork(SIGCHLD, regs->gpr[1], regs, 0, NULL, NULL);
}

int sys_vfork(unsigned long p1, unsigned long p2, unsigned long p3,
	      unsigned long p4, unsigned long p5, unsigned long p6,
	      struct pt_regs *regs)
{
	CHECK_FULL_REGS(regs);
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs->gpr[1],
			regs, 0, NULL, NULL);
}

int sys_execve(unsigned long a0, unsigned long a1, unsigned long a2,
	       unsigned long a3, unsigned long a4, unsigned long a5,
	       struct pt_regs *regs)
{
	int error;
	char *filename;

	filename = getname((const char __user *) a0);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	flush_fp_to_thread(current);
	flush_altivec_to_thread(current);
	flush_spe_to_thread(current);
	error = do_execve(filename,
			  (const char __user *const __user *) a1,
			  (const char __user *const __user *) a2, regs);
	putname(filename);
out:
	return error;
}

static inline int valid_irq_stack(unsigned long sp, struct task_struct *p,
				  unsigned long nbytes)
{
	unsigned long stack_page;
	unsigned long cpu = task_cpu(p);

	if (cpu < NR_CPUS && cpu_possible(cpu)) {
		stack_page = (unsigned long) hardirq_ctx[cpu];
		if (sp >= stack_page + sizeof(struct thread_struct)
		    && sp <= stack_page + THREAD_SIZE - nbytes)
			return 1;

		stack_page = (unsigned long) softirq_ctx[cpu];
		if (sp >= stack_page + sizeof(struct thread_struct)
		    && sp <= stack_page + THREAD_SIZE - nbytes)
			return 1;
	}
	return 0;
}

int validate_sp(unsigned long sp, struct task_struct *p,
		       unsigned long nbytes)
{
	unsigned long stack_page = (unsigned long)task_stack_page(p);

	if (sp >= stack_page + sizeof(struct thread_struct)
	    && sp <= stack_page + THREAD_SIZE - nbytes)
		return 1;

	return valid_irq_stack(sp, p, nbytes);
}

EXPORT_SYMBOL(validate_sp);

unsigned long get_wchan(struct task_struct *p)
{
	unsigned long ip, sp;
	int count = 0;

	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	sp = p->thread.ksp;
	if (!validate_sp(sp, p, STACK_FRAME_OVERHEAD))
		return 0;

	do {
		sp = *(unsigned long *)sp;
		if (!validate_sp(sp, p, STACK_FRAME_OVERHEAD))
			return 0;
		if (count > 0) {
			ip = ((unsigned long *)sp)[STACK_FRAME_LR_SAVE];
			if (!in_sched_functions(ip))
				return ip;
		}
	} while (count++ < 16);
	return 0;
}

static int kstack_depth_to_print = CONFIG_PRINT_STACK_DEPTH;

void show_stack(struct task_struct *tsk, unsigned long *stack)
{
	unsigned long sp, ip, lr, newsp;
	int count = 0;
	int firstframe = 1;
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	int curr_frame = current->curr_ret_stack;
	extern void return_to_handler(void);
	unsigned long rth = (unsigned long)return_to_handler;
	unsigned long mrth = -1;
#ifdef CONFIG_PPC64
	extern void mod_return_to_handler(void);
	rth = *(unsigned long *)rth;
	mrth = (unsigned long)mod_return_to_handler;
	mrth = *(unsigned long *)mrth;
#endif
#endif

	sp = (unsigned long) stack;
	if (tsk == NULL)
		tsk = current;
	if (sp == 0) {
		if (tsk == current)
			asm("mr %0,1" : "=r" (sp));
		else
			sp = tsk->thread.ksp;
	}

	lr = 0;
	printk("Call Trace:\n");
	do {
		if (!validate_sp(sp, tsk, STACK_FRAME_OVERHEAD))
			return;

		stack = (unsigned long *) sp;
		newsp = stack[0];
		ip = stack[STACK_FRAME_LR_SAVE];
		if (!firstframe || ip != lr) {
			printk("["REG"] ["REG"] %pS", sp, ip, (void *)ip);
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
			if ((ip == rth || ip == mrth) && curr_frame >= 0) {
				printk(" (%pS)",
				       (void *)current->ret_stack[curr_frame].ret);
				curr_frame--;
			}
#endif
			if (firstframe)
				printk(" (unreliable)");
			printk("\n");
		}
		firstframe = 0;

		if (validate_sp(sp, tsk, STACK_INT_FRAME_SIZE)
		    && stack[STACK_FRAME_MARKER] == STACK_FRAME_REGS_MARKER) {
			struct pt_regs *regs = (struct pt_regs *)
				(sp + STACK_FRAME_OVERHEAD);
			lr = regs->link;
			printk("--- Exception: %lx at %pS\n    LR = %pS\n",
			       regs->trap, (void *)regs->nip, (void *)lr);
			firstframe = 1;
		}

		sp = newsp;
	} while (count++ < kstack_depth_to_print);
}

void dump_stack(void)
{
	show_stack(current, NULL);
}
EXPORT_SYMBOL(dump_stack);

#ifdef CONFIG_PPC64
void __ppc64_runlatch_on(void)
{
	struct thread_info *ti = current_thread_info();
	unsigned long ctrl;

	ctrl = mfspr(SPRN_CTRLF);
	ctrl |= CTRL_RUNLATCH;
	mtspr(SPRN_CTRLT, ctrl);

	ti->local_flags |= _TLF_RUNLATCH;
}

void __ppc64_runlatch_off(void)
{
	struct thread_info *ti = current_thread_info();
	unsigned long ctrl;

	ti->local_flags &= ~_TLF_RUNLATCH;

	ctrl = mfspr(SPRN_CTRLF);
	ctrl &= ~CTRL_RUNLATCH;
	mtspr(SPRN_CTRLT, ctrl);
}
#endif 

#if THREAD_SHIFT < PAGE_SHIFT

static struct kmem_cache *thread_info_cache;

struct thread_info *alloc_thread_info_node(struct task_struct *tsk, int node)
{
	struct thread_info *ti;

	ti = kmem_cache_alloc_node(thread_info_cache, GFP_KERNEL, node);
	if (unlikely(ti == NULL))
		return NULL;
#ifdef CONFIG_DEBUG_STACK_USAGE
	memset(ti, 0, THREAD_SIZE);
#endif
	return ti;
}

void free_thread_info(struct thread_info *ti)
{
	kmem_cache_free(thread_info_cache, ti);
}

void thread_info_cache_init(void)
{
	thread_info_cache = kmem_cache_create("thread_info", THREAD_SIZE,
					      THREAD_SIZE, 0, NULL);
	BUG_ON(thread_info_cache == NULL);
}

#endif 

unsigned long arch_align_stack(unsigned long sp)
{
	if (!(current->personality & ADDR_NO_RANDOMIZE) && randomize_va_space)
		sp -= get_random_int() & ~PAGE_MASK;
	return sp & ~0xf;
}

static inline unsigned long brk_rnd(void)
{
        unsigned long rnd = 0;

	
	if (is_32bit_task())
		rnd = (long)(get_random_int() % (1<<(23-PAGE_SHIFT)));
	else
		rnd = (long)(get_random_int() % (1<<(30-PAGE_SHIFT)));

	return rnd << PAGE_SHIFT;
}

unsigned long arch_randomize_brk(struct mm_struct *mm)
{
	unsigned long base = mm->brk;
	unsigned long ret;

#ifdef CONFIG_PPC_STD_MMU_64
	if (!is_32bit_task() && (mmu_highuser_ssize == MMU_SEGSIZE_1T))
		base = max_t(unsigned long, mm->brk, 1UL << SID_SHIFT_1T);
#endif

	ret = PAGE_ALIGN(base + brk_rnd());

	if (ret < mm->brk)
		return mm->brk;

	return ret;
}

unsigned long randomize_et_dyn(unsigned long base)
{
	unsigned long ret = PAGE_ALIGN(base + brk_rnd());

	if (ret < base)
		return base;

	return ret;
}
