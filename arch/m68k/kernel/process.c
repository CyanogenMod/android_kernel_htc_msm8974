/*
 *  linux/arch/m68k/kernel/process.c
 *
 *  Copyright (C) 1995  Hamish Macdonald
 *
 *  68060 fixes by Jesper Skov
 */


#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/smp.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/reboot.h>
#include <linux/init_task.h>
#include <linux/mqueue.h>

#include <asm/uaccess.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/setup.h>
#include <asm/pgtable.h>


asmlinkage void ret_from_fork(void);


unsigned long thread_saved_pc(struct task_struct *tsk)
{
	struct switch_stack *sw = (struct switch_stack *)tsk->thread.ksp;
	
	if (in_sched_functions(sw->retpc))
		return ((unsigned long *)sw->a6)[1];
	else
		return sw->retpc;
}

static void default_idle(void)
{
	if (!need_resched())
#if defined(MACH_ATARI_ONLY)
		
		__asm__("stop #0x2200" : : : "cc");
#else
		__asm__("stop #0x2000" : : : "cc");
#endif
}

void (*idle)(void) = default_idle;

void cpu_idle(void)
{
	
	while (1) {
		while (!need_resched())
			idle();
		schedule_preempt_disabled();
	}
}

void machine_restart(char * __unused)
{
	if (mach_reset)
		mach_reset();
	for (;;);
}

void machine_halt(void)
{
	if (mach_halt)
		mach_halt();
	for (;;);
}

void machine_power_off(void)
{
	if (mach_power_off)
		mach_power_off();
	for (;;);
}

void (*pm_power_off)(void) = machine_power_off;
EXPORT_SYMBOL(pm_power_off);

void show_regs(struct pt_regs * regs)
{
	printk("\n");
	printk("Format %02x  Vector: %04x  PC: %08lx  Status: %04x    %s\n",
	       regs->format, regs->vector, regs->pc, regs->sr, print_tainted());
	printk("ORIG_D0: %08lx  D0: %08lx  A2: %08lx  A1: %08lx\n",
	       regs->orig_d0, regs->d0, regs->a2, regs->a1);
	printk("A0: %08lx  D5: %08lx  D4: %08lx\n",
	       regs->a0, regs->d5, regs->d4);
	printk("D3: %08lx  D2: %08lx  D1: %08lx\n",
	       regs->d3, regs->d2, regs->d1);
	if (!(regs->sr & PS_S))
		printk("USP: %08lx\n", rdusp());
}

int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	int pid;
	mm_segment_t fs;

	fs = get_fs();
	set_fs (KERNEL_DS);

	{
	register long retval __asm__ ("d0");
	register long clone_arg __asm__ ("d1") = flags | CLONE_VM | CLONE_UNTRACED;

	retval = __NR_clone;
	__asm__ __volatile__
	  ("clrl %%d2\n\t"
	   "trap #0\n\t"		
	   "tstl %0\n\t"		
	   "jne 1f\n\t"			
#ifdef CONFIG_MMU
	   "lea %%sp@(%c7),%6\n\t"	
	   "movel %6@,%6\n\t"
#endif
	   "movel %3,%%sp@-\n\t"	
	   "jsr %4@\n\t"		
	   "movel %0,%%d1\n\t"		
	   "movel %2,%%d0\n\t"		
	   "trap #0\n"
	   "1:"
	   : "+d" (retval)
	   : "i" (__NR_clone), "i" (__NR_exit),
	     "r" (arg), "a" (fn), "d" (clone_arg), "r" (current),
	     "i" (-THREAD_SIZE)
	   : "d2");

	pid = retval;
	}

	set_fs (fs);
	return pid;
}
EXPORT_SYMBOL(kernel_thread);

void flush_thread(void)
{
	current->thread.fs = __USER_DS;
#ifdef CONFIG_FPU
	if (!FPU_IS_EMU) {
		unsigned long zero = 0;
		asm volatile("frestore %0": :"m" (zero));
	}
#endif
}


asmlinkage int m68k_fork(struct pt_regs *regs)
{
#ifdef CONFIG_MMU
	return do_fork(SIGCHLD, rdusp(), regs, 0, NULL, NULL);
#else
	return -EINVAL;
#endif
}

asmlinkage int m68k_vfork(struct pt_regs *regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, rdusp(), regs, 0,
		       NULL, NULL);
}

asmlinkage int m68k_clone(struct pt_regs *regs)
{
	unsigned long clone_flags;
	unsigned long newsp;
	int __user *parent_tidptr, *child_tidptr;

	
	clone_flags = regs->d1;
	newsp = regs->d2;
	parent_tidptr = (int __user *)regs->d3;
	child_tidptr = (int __user *)regs->d4;
	if (!newsp)
		newsp = rdusp();
	return do_fork(clone_flags, newsp, regs, 0,
		       parent_tidptr, child_tidptr);
}

int copy_thread(unsigned long clone_flags, unsigned long usp,
		 unsigned long unused,
		 struct task_struct * p, struct pt_regs * regs)
{
	struct pt_regs * childregs;
	struct switch_stack * childstack, *stack;
	unsigned long *retp;

	childregs = (struct pt_regs *) (task_stack_page(p) + THREAD_SIZE) - 1;

	*childregs = *regs;
	childregs->d0 = 0;

	retp = ((unsigned long *) regs);
	stack = ((struct switch_stack *) retp) - 1;

	childstack = ((struct switch_stack *) childregs) - 1;
	*childstack = *stack;
	childstack->retpc = (unsigned long)ret_from_fork;

	p->thread.usp = usp;
	p->thread.ksp = (unsigned long)childstack;

	if (clone_flags & CLONE_SETTLS)
		task_thread_info(p)->tp_value = regs->d5;

	p->thread.fs = get_fs().seg;

#ifdef CONFIG_FPU
	if (!FPU_IS_EMU) {
		
		asm volatile ("fsave %0" : : "m" (p->thread.fpstate[0]) : "memory");

		if (!CPU_IS_060 ? p->thread.fpstate[0] : p->thread.fpstate[2]) {
			if (CPU_IS_COLDFIRE) {
				asm volatile ("fmovemd %/fp0-%/fp7,%0\n\t"
					      "fmovel %/fpiar,%1\n\t"
					      "fmovel %/fpcr,%2\n\t"
					      "fmovel %/fpsr,%3"
					      :
					      : "m" (p->thread.fp[0]),
						"m" (p->thread.fpcntl[0]),
						"m" (p->thread.fpcntl[1]),
						"m" (p->thread.fpcntl[2])
					      : "memory");
			} else {
				asm volatile ("fmovemx %/fp0-%/fp7,%0\n\t"
					      "fmoveml %/fpiar/%/fpcr/%/fpsr,%1"
					      :
					      : "m" (p->thread.fp[0]),
						"m" (p->thread.fpcntl[0])
					      : "memory");
			}
		}

		
		asm volatile ("frestore %0" : : "m" (p->thread.fpstate[0]));
	}
#endif 

	return 0;
}

#ifdef CONFIG_FPU
int dump_fpu (struct pt_regs *regs, struct user_m68kfp_struct *fpu)
{
	char fpustate[216];

	if (FPU_IS_EMU) {
		int i;

		memcpy(fpu->fpcntl, current->thread.fpcntl, 12);
		memcpy(fpu->fpregs, current->thread.fp, 96);
		for (i = 0; i < 24; i += 3)
			fpu->fpregs[i] = ((fpu->fpregs[i] & 0xffff0000) << 15) |
			                 ((fpu->fpregs[i] & 0x0000ffff) << 16);
		return 1;
	}

	
	asm volatile ("fsave %0" :: "m" (fpustate[0]) : "memory");
	if (!CPU_IS_060 ? !fpustate[0] : !fpustate[2])
		return 0;

	if (CPU_IS_COLDFIRE) {
		asm volatile ("fmovel %/fpiar,%0\n\t"
			      "fmovel %/fpcr,%1\n\t"
			      "fmovel %/fpsr,%2\n\t"
			      "fmovemd %/fp0-%/fp7,%3"
			      :
			      : "m" (fpu->fpcntl[0]),
				"m" (fpu->fpcntl[1]),
				"m" (fpu->fpcntl[2]),
				"m" (fpu->fpregs[0])
			      : "memory");
	} else {
		asm volatile ("fmovem %/fpiar/%/fpcr/%/fpsr,%0"
			      :
			      : "m" (fpu->fpcntl[0])
			      : "memory");
		asm volatile ("fmovemx %/fp0-%/fp7,%0"
			      :
			      : "m" (fpu->fpregs[0])
			      : "memory");
	}

	return 1;
}
EXPORT_SYMBOL(dump_fpu);
#endif 

asmlinkage int sys_execve(const char __user *name,
			  const char __user *const __user *argv,
			  const char __user *const __user *envp)
{
	int error;
	char * filename;
	struct pt_regs *regs = (struct pt_regs *) &name;

	filename = getname(name);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		return error;
	error = do_execve(filename, argv, envp, regs);
	putname(filename);
	return error;
}

unsigned long get_wchan(struct task_struct *p)
{
	unsigned long fp, pc;
	unsigned long stack_page;
	int count = 0;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	stack_page = (unsigned long)task_stack_page(p);
	fp = ((struct switch_stack *)p->thread.ksp)->a6;
	do {
		if (fp < stack_page+sizeof(struct thread_info) ||
		    fp >= 8184+stack_page)
			return 0;
		pc = ((unsigned long *)fp)[1];
		if (!in_sched_functions(pc))
			return pc;
		fp = *(unsigned long *) fp;
	} while (count++ < 16);
	return 0;
}
