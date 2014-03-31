/*
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 2000, 2001, 2002 Andi Kleen, SuSE Labs
 *
 *  Pentium III FXSR, SSE support
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 */

#include <linux/interrupt.h>
#include <linux/kallsyms.h>
#include <linux/spinlock.h>
#include <linux/kprobes.h>
#include <linux/uaccess.h>
#include <linux/kdebug.h>
#include <linux/kgdb.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ptrace.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kexec.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/bug.h>
#include <linux/nmi.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/io.h>

#ifdef CONFIG_EISA
#include <linux/ioport.h>
#include <linux/eisa.h>
#endif

#ifdef CONFIG_MCA
#include <linux/mca.h>
#endif

#if defined(CONFIG_EDAC)
#include <linux/edac.h>
#endif

#include <asm/kmemcheck.h>
#include <asm/stacktrace.h>
#include <asm/processor.h>
#include <asm/debugreg.h>
#include <linux/atomic.h>
#include <asm/traps.h>
#include <asm/desc.h>
#include <asm/i387.h>
#include <asm/fpu-internal.h>
#include <asm/mce.h>

#include <asm/mach_traps.h>

#ifdef CONFIG_X86_64
#include <asm/x86_init.h>
#include <asm/pgalloc.h>
#include <asm/proto.h>
#else
#include <asm/processor-flags.h>
#include <asm/setup.h>

asmlinkage int system_call(void);

char ignore_fpu_irq;

gate_desc idt_table[NR_VECTORS] __page_aligned_data = { { { { 0, 0 } } }, };
#endif

DECLARE_BITMAP(used_vectors, NR_VECTORS);
EXPORT_SYMBOL_GPL(used_vectors);

static inline void conditional_sti(struct pt_regs *regs)
{
	if (regs->flags & X86_EFLAGS_IF)
		local_irq_enable();
}

static inline void preempt_conditional_sti(struct pt_regs *regs)
{
	inc_preempt_count();
	if (regs->flags & X86_EFLAGS_IF)
		local_irq_enable();
}

static inline void conditional_cli(struct pt_regs *regs)
{
	if (regs->flags & X86_EFLAGS_IF)
		local_irq_disable();
}

static inline void preempt_conditional_cli(struct pt_regs *regs)
{
	if (regs->flags & X86_EFLAGS_IF)
		local_irq_disable();
	dec_preempt_count();
}

static void __kprobes
do_trap(int trapnr, int signr, char *str, struct pt_regs *regs,
	long error_code, siginfo_t *info)
{
	struct task_struct *tsk = current;

#ifdef CONFIG_X86_32
	if (regs->flags & X86_VM_MASK) {
		if (trapnr < X86_TRAP_UD)
			goto vm86_trap;
		goto trap_signal;
	}
#endif

	if (!user_mode(regs))
		goto kernel_trap;

#ifdef CONFIG_X86_32
trap_signal:
#endif
	tsk->thread.error_code = error_code;
	tsk->thread.trap_nr = trapnr;

#ifdef CONFIG_X86_64
	if (show_unhandled_signals && unhandled_signal(tsk, signr) &&
	    printk_ratelimit()) {
		printk(KERN_INFO
		       "%s[%d] trap %s ip:%lx sp:%lx error:%lx",
		       tsk->comm, tsk->pid, str,
		       regs->ip, regs->sp, error_code);
		print_vma_addr(" in ", regs->ip);
		printk("\n");
	}
#endif

	if (info)
		force_sig_info(signr, info, tsk);
	else
		force_sig(signr, tsk);
	return;

kernel_trap:
	if (!fixup_exception(regs)) {
		tsk->thread.error_code = error_code;
		tsk->thread.trap_nr = trapnr;
		die(str, regs, error_code);
	}
	return;

#ifdef CONFIG_X86_32
vm86_trap:
	if (handle_vm86_trap((struct kernel_vm86_regs *) regs,
						error_code, trapnr))
		goto trap_signal;
	return;
#endif
}

#define DO_ERROR(trapnr, signr, str, name)				\
dotraplinkage void do_##name(struct pt_regs *regs, long error_code)	\
{									\
	if (notify_die(DIE_TRAP, str, regs, error_code, trapnr, signr)	\
							== NOTIFY_STOP)	\
		return;							\
	conditional_sti(regs);						\
	do_trap(trapnr, signr, str, regs, error_code, NULL);		\
}

#define DO_ERROR_INFO(trapnr, signr, str, name, sicode, siaddr)		\
dotraplinkage void do_##name(struct pt_regs *regs, long error_code)	\
{									\
	siginfo_t info;							\
	info.si_signo = signr;						\
	info.si_errno = 0;						\
	info.si_code = sicode;						\
	info.si_addr = (void __user *)siaddr;				\
	if (notify_die(DIE_TRAP, str, regs, error_code, trapnr, signr)	\
							== NOTIFY_STOP)	\
		return;							\
	conditional_sti(regs);						\
	do_trap(trapnr, signr, str, regs, error_code, &info);		\
}

DO_ERROR_INFO(X86_TRAP_DE, SIGFPE, "divide error", divide_error, FPE_INTDIV,
		regs->ip)
DO_ERROR(X86_TRAP_OF, SIGSEGV, "overflow", overflow)
DO_ERROR(X86_TRAP_BR, SIGSEGV, "bounds", bounds)
DO_ERROR_INFO(X86_TRAP_UD, SIGILL, "invalid opcode", invalid_op, ILL_ILLOPN,
		regs->ip)
DO_ERROR(X86_TRAP_OLD_MF, SIGFPE, "coprocessor segment overrun",
		coprocessor_segment_overrun)
DO_ERROR(X86_TRAP_TS, SIGSEGV, "invalid TSS", invalid_TSS)
DO_ERROR(X86_TRAP_NP, SIGBUS, "segment not present", segment_not_present)
#ifdef CONFIG_X86_32
DO_ERROR(X86_TRAP_SS, SIGBUS, "stack segment", stack_segment)
#endif
DO_ERROR_INFO(X86_TRAP_AC, SIGBUS, "alignment check", alignment_check,
		BUS_ADRALN, 0)

#ifdef CONFIG_X86_64
dotraplinkage void do_stack_segment(struct pt_regs *regs, long error_code)
{
	if (notify_die(DIE_TRAP, "stack segment", regs, error_code,
			X86_TRAP_SS, SIGBUS) == NOTIFY_STOP)
		return;
	preempt_conditional_sti(regs);
	do_trap(X86_TRAP_SS, SIGBUS, "stack segment", regs, error_code, NULL);
	preempt_conditional_cli(regs);
}

dotraplinkage void do_double_fault(struct pt_regs *regs, long error_code)
{
	static const char str[] = "double fault";
	struct task_struct *tsk = current;

	
	notify_die(DIE_TRAP, str, regs, error_code, X86_TRAP_DF, SIGSEGV);

	tsk->thread.error_code = error_code;
	tsk->thread.trap_nr = X86_TRAP_DF;

	for (;;)
		die(str, regs, error_code);
}
#endif

dotraplinkage void __kprobes
do_general_protection(struct pt_regs *regs, long error_code)
{
	struct task_struct *tsk;

	conditional_sti(regs);

#ifdef CONFIG_X86_32
	if (regs->flags & X86_VM_MASK)
		goto gp_in_vm86;
#endif

	tsk = current;
	if (!user_mode(regs))
		goto gp_in_kernel;

	tsk->thread.error_code = error_code;
	tsk->thread.trap_nr = X86_TRAP_GP;

	if (show_unhandled_signals && unhandled_signal(tsk, SIGSEGV) &&
			printk_ratelimit()) {
		printk(KERN_INFO
			"%s[%d] general protection ip:%lx sp:%lx error:%lx",
			tsk->comm, task_pid_nr(tsk),
			regs->ip, regs->sp, error_code);
		print_vma_addr(" in ", regs->ip);
		printk("\n");
	}

	force_sig(SIGSEGV, tsk);
	return;

#ifdef CONFIG_X86_32
gp_in_vm86:
	local_irq_enable();
	handle_vm86_fault((struct kernel_vm86_regs *) regs, error_code);
	return;
#endif

gp_in_kernel:
	if (fixup_exception(regs))
		return;

	tsk->thread.error_code = error_code;
	tsk->thread.trap_nr = X86_TRAP_GP;
	if (notify_die(DIE_GPF, "general protection fault", regs, error_code,
			X86_TRAP_GP, SIGSEGV) == NOTIFY_STOP)
		return;
	die("general protection fault", regs, error_code);
}

dotraplinkage void __kprobes do_int3(struct pt_regs *regs, long error_code)
{
#ifdef CONFIG_KGDB_LOW_LEVEL_TRAP
	if (kgdb_ll_trap(DIE_INT3, "int3", regs, error_code, X86_TRAP_BP,
				SIGTRAP) == NOTIFY_STOP)
		return;
#endif 

	if (notify_die(DIE_INT3, "int3", regs, error_code, X86_TRAP_BP,
			SIGTRAP) == NOTIFY_STOP)
		return;

	debug_stack_usage_inc();
	preempt_conditional_sti(regs);
	do_trap(X86_TRAP_BP, SIGTRAP, "int3", regs, error_code, NULL);
	preempt_conditional_cli(regs);
	debug_stack_usage_dec();
}

#ifdef CONFIG_X86_64
asmlinkage __kprobes struct pt_regs *sync_regs(struct pt_regs *eregs)
{
	struct pt_regs *regs = eregs;
	
	if (eregs == (struct pt_regs *)eregs->sp)
		;
	
	else if (user_mode(eregs))
		regs = task_pt_regs(current);
	else if (eregs->flags & X86_EFLAGS_IF)
		regs = (struct pt_regs *)(eregs->sp -= sizeof(struct pt_regs));
	if (eregs != regs)
		*regs = *eregs;
	return regs;
}
#endif

dotraplinkage void __kprobes do_debug(struct pt_regs *regs, long error_code)
{
	struct task_struct *tsk = current;
	int user_icebp = 0;
	unsigned long dr6;
	int si_code;

	get_debugreg(dr6, 6);

	
	dr6 &= ~DR6_RESERVED;

	if (!dr6 && user_mode(regs))
		user_icebp = 1;

	
	if ((dr6 & DR_STEP) && kmemcheck_trap(regs))
		return;

	
	set_debugreg(0, 6);

	clear_tsk_thread_flag(tsk, TIF_BLOCKSTEP);

	
	tsk->thread.debugreg6 = dr6;

	if (notify_die(DIE_DEBUG, "debug", regs, PTR_ERR(&dr6), error_code,
							SIGTRAP) == NOTIFY_STOP)
		return;

	debug_stack_usage_inc();

	
	preempt_conditional_sti(regs);

	if (regs->flags & X86_VM_MASK) {
		handle_vm86_trap((struct kernel_vm86_regs *) regs, error_code,
					X86_TRAP_DB);
		preempt_conditional_cli(regs);
		debug_stack_usage_dec();
		return;
	}

	if ((dr6 & DR_STEP) && !user_mode(regs)) {
		tsk->thread.debugreg6 &= ~DR_STEP;
		set_tsk_thread_flag(tsk, TIF_SINGLESTEP);
		regs->flags &= ~X86_EFLAGS_TF;
	}
	si_code = get_si_code(tsk->thread.debugreg6);
	if (tsk->thread.debugreg6 & (DR_STEP | DR_TRAP_BITS) || user_icebp)
		send_sigtrap(tsk, regs, error_code, si_code);
	preempt_conditional_cli(regs);
	debug_stack_usage_dec();

	return;
}

void math_error(struct pt_regs *regs, int error_code, int trapnr)
{
	struct task_struct *task = current;
	siginfo_t info;
	unsigned short err;
	char *str = (trapnr == X86_TRAP_MF) ? "fpu exception" :
						"simd exception";

	if (notify_die(DIE_TRAP, str, regs, error_code, trapnr, SIGFPE) == NOTIFY_STOP)
		return;
	conditional_sti(regs);

	if (!user_mode_vm(regs))
	{
		if (!fixup_exception(regs)) {
			task->thread.error_code = error_code;
			task->thread.trap_nr = trapnr;
			die(str, regs, error_code);
		}
		return;
	}

	save_init_fpu(task);
	task->thread.trap_nr = trapnr;
	task->thread.error_code = error_code;
	info.si_signo = SIGFPE;
	info.si_errno = 0;
	info.si_addr = (void __user *)regs->ip;
	if (trapnr == X86_TRAP_MF) {
		unsigned short cwd, swd;
		cwd = get_fpu_cwd(task);
		swd = get_fpu_swd(task);

		err = swd & ~cwd;
	} else {
		unsigned short mxcsr = get_fpu_mxcsr(task);
		err = ~(mxcsr >> 7) & mxcsr;
	}

	if (err & 0x001) {	
		info.si_code = FPE_FLTINV;
	} else if (err & 0x004) { 
		info.si_code = FPE_FLTDIV;
	} else if (err & 0x008) { 
		info.si_code = FPE_FLTOVF;
	} else if (err & 0x012) { 
		info.si_code = FPE_FLTUND;
	} else if (err & 0x020) { 
		info.si_code = FPE_FLTRES;
	} else {
		return;
	}
	force_sig_info(SIGFPE, &info, task);
}

dotraplinkage void do_coprocessor_error(struct pt_regs *regs, long error_code)
{
#ifdef CONFIG_X86_32
	ignore_fpu_irq = 1;
#endif

	math_error(regs, error_code, X86_TRAP_MF);
}

dotraplinkage void
do_simd_coprocessor_error(struct pt_regs *regs, long error_code)
{
	math_error(regs, error_code, X86_TRAP_XF);
}

dotraplinkage void
do_spurious_interrupt_bug(struct pt_regs *regs, long error_code)
{
	conditional_sti(regs);
#if 0
	
	printk(KERN_INFO "Ignoring P6 Local APIC Spurious Interrupt Bug...\n");
#endif
}

asmlinkage void __attribute__((weak)) smp_thermal_interrupt(void)
{
}

asmlinkage void __attribute__((weak)) smp_threshold_interrupt(void)
{
}

void math_state_restore(void)
{
	struct task_struct *tsk = current;

	if (!tsk_used_math(tsk)) {
		local_irq_enable();
		if (init_fpu(tsk)) {
			do_group_exit(SIGKILL);
			return;
		}
		local_irq_disable();
	}

	__thread_fpu_begin(tsk);
	if (unlikely(restore_fpu_checking(tsk))) {
		__thread_fpu_end(tsk);
		force_sig(SIGSEGV, tsk);
		return;
	}

	tsk->fpu_counter++;
}
EXPORT_SYMBOL_GPL(math_state_restore);

dotraplinkage void __kprobes
do_device_not_available(struct pt_regs *regs, long error_code)
{
#ifdef CONFIG_MATH_EMULATION
	if (read_cr0() & X86_CR0_EM) {
		struct math_emu_info info = { };

		conditional_sti(regs);

		info.regs = regs;
		math_emulate(&info);
		return;
	}
#endif
	math_state_restore(); 
#ifdef CONFIG_X86_32
	conditional_sti(regs);
#endif
}

#ifdef CONFIG_X86_32
dotraplinkage void do_iret_error(struct pt_regs *regs, long error_code)
{
	siginfo_t info;
	local_irq_enable();

	info.si_signo = SIGILL;
	info.si_errno = 0;
	info.si_code = ILL_BADSTK;
	info.si_addr = NULL;
	if (notify_die(DIE_TRAP, "iret exception", regs, error_code,
			X86_TRAP_IRET, SIGILL) == NOTIFY_STOP)
		return;
	do_trap(X86_TRAP_IRET, SIGILL, "iret exception", regs, error_code,
		&info);
}
#endif

void __init early_trap_init(void)
{
	set_intr_gate_ist(X86_TRAP_DB, &debug, DEBUG_STACK);
	
	set_system_intr_gate_ist(X86_TRAP_BP, &int3, DEBUG_STACK);
	set_intr_gate(X86_TRAP_PF, &page_fault);
	load_idt(&idt_descr);
}

void __init trap_init(void)
{
	int i;

#ifdef CONFIG_EISA
	void __iomem *p = early_ioremap(0x0FFFD9, 4);

	if (readl(p) == 'E' + ('I'<<8) + ('S'<<16) + ('A'<<24))
		EISA_bus = 1;
	early_iounmap(p, 4);
#endif

	set_intr_gate(X86_TRAP_DE, &divide_error);
	set_intr_gate_ist(X86_TRAP_NMI, &nmi, NMI_STACK);
	
	set_system_intr_gate(X86_TRAP_OF, &overflow);
	set_intr_gate(X86_TRAP_BR, &bounds);
	set_intr_gate(X86_TRAP_UD, &invalid_op);
	set_intr_gate(X86_TRAP_NM, &device_not_available);
#ifdef CONFIG_X86_32
	set_task_gate(X86_TRAP_DF, GDT_ENTRY_DOUBLEFAULT_TSS);
#else
	set_intr_gate_ist(X86_TRAP_DF, &double_fault, DOUBLEFAULT_STACK);
#endif
	set_intr_gate(X86_TRAP_OLD_MF, &coprocessor_segment_overrun);
	set_intr_gate(X86_TRAP_TS, &invalid_TSS);
	set_intr_gate(X86_TRAP_NP, &segment_not_present);
	set_intr_gate_ist(X86_TRAP_SS, &stack_segment, STACKFAULT_STACK);
	set_intr_gate(X86_TRAP_GP, &general_protection);
	set_intr_gate(X86_TRAP_SPURIOUS, &spurious_interrupt_bug);
	set_intr_gate(X86_TRAP_MF, &coprocessor_error);
	set_intr_gate(X86_TRAP_AC, &alignment_check);
#ifdef CONFIG_X86_MCE
	set_intr_gate_ist(X86_TRAP_MC, &machine_check, MCE_STACK);
#endif
	set_intr_gate(X86_TRAP_XF, &simd_coprocessor_error);

	
	for (i = 0; i < FIRST_EXTERNAL_VECTOR; i++)
		set_bit(i, used_vectors);

#ifdef CONFIG_IA32_EMULATION
	set_system_intr_gate(IA32_SYSCALL_VECTOR, ia32_syscall);
	set_bit(IA32_SYSCALL_VECTOR, used_vectors);
#endif

#ifdef CONFIG_X86_32
	set_system_trap_gate(SYSCALL_VECTOR, &system_call);
	set_bit(SYSCALL_VECTOR, used_vectors);
#endif

	cpu_init();

	x86_init.irqs.trap_init();

#ifdef CONFIG_X86_64
	memcpy(&nmi_idt_table, &idt_table, IDT_ENTRIES * 16);
	set_nmi_gate(X86_TRAP_DB, &debug);
	set_nmi_gate(X86_TRAP_BP, &int3);
#endif
}
