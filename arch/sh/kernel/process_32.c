/*
 * arch/sh/kernel/process.c
 *
 * This file handles the architecture-dependent parts of process handling..
 *
 *  Copyright (C) 1995  Linus Torvalds
 *
 *  SuperH version:  Copyright (C) 1999, 2000  Niibe Yutaka & Kaz Kojima
 *		     Copyright (C) 2006 Lineo Solutions Inc. support SH4A UBC
 *		     Copyright (C) 2002 - 2008  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/elfcore.h>
#include <linux/kallsyms.h>
#include <linux/fs.h>
#include <linux/ftrace.h>
#include <linux/hw_breakpoint.h>
#include <linux/prefetch.h>
#include <asm/uaccess.h>
#include <asm/mmu_context.h>
#include <asm/fpu.h>
#include <asm/syscalls.h>
#include <asm/switch_to.h>

void show_regs(struct pt_regs * regs)
{
	printk("\n");
	printk("Pid : %d, Comm: \t\t%s\n", task_pid_nr(current), current->comm);
	printk("CPU : %d        \t\t%s  (%s %.*s)\n\n",
	       smp_processor_id(), print_tainted(), init_utsname()->release,
	       (int)strcspn(init_utsname()->version, " "),
	       init_utsname()->version);

	print_symbol("PC is at %s\n", instruction_pointer(regs));
	print_symbol("PR is at %s\n", regs->pr);

	printk("PC  : %08lx SP  : %08lx SR  : %08lx ",
	       regs->pc, regs->regs[15], regs->sr);
#ifdef CONFIG_MMU
	printk("TEA : %08x\n", __raw_readl(MMU_TEA));
#else
	printk("\n");
#endif

	printk("R0  : %08lx R1  : %08lx R2  : %08lx R3  : %08lx\n",
	       regs->regs[0],regs->regs[1],
	       regs->regs[2],regs->regs[3]);
	printk("R4  : %08lx R5  : %08lx R6  : %08lx R7  : %08lx\n",
	       regs->regs[4],regs->regs[5],
	       regs->regs[6],regs->regs[7]);
	printk("R8  : %08lx R9  : %08lx R10 : %08lx R11 : %08lx\n",
	       regs->regs[8],regs->regs[9],
	       regs->regs[10],regs->regs[11]);
	printk("R12 : %08lx R13 : %08lx R14 : %08lx\n",
	       regs->regs[12],regs->regs[13],
	       regs->regs[14]);
	printk("MACH: %08lx MACL: %08lx GBR : %08lx PR  : %08lx\n",
	       regs->mach, regs->macl, regs->gbr, regs->pr);

	show_trace(NULL, (unsigned long *)regs->regs[15], regs);
	show_code(regs);
}

__noreturn void kernel_thread_helper(void *arg, int (*fn)(void *))
{
	do_exit(fn(arg));
}

int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	struct pt_regs regs;
	int pid;

	memset(&regs, 0, sizeof(regs));
	regs.regs[4] = (unsigned long)arg;
	regs.regs[5] = (unsigned long)fn;

	regs.pc = (unsigned long)kernel_thread_helper;
	regs.sr = SR_MD;
#if defined(CONFIG_SH_FPU)
	regs.sr |= SR_FD;
#endif

	
	pid = do_fork(flags | CLONE_VM | CLONE_UNTRACED, 0,
		      &regs, 0, NULL, NULL);

	return pid;
}
EXPORT_SYMBOL(kernel_thread);

void start_thread(struct pt_regs *regs, unsigned long new_pc,
		  unsigned long new_sp)
{
	regs->pr = 0;
	regs->sr = SR_FD;
	regs->pc = new_pc;
	regs->regs[15] = new_sp;

	free_thread_xstate(current);
}
EXPORT_SYMBOL(start_thread);

void exit_thread(void)
{
}

void flush_thread(void)
{
	struct task_struct *tsk = current;

	flush_ptrace_hw_breakpoint(tsk);

#if defined(CONFIG_SH_FPU)
	
	clear_fpu(tsk, task_pt_regs(tsk));
	clear_used_math();
#endif
}

void release_thread(struct task_struct *dead_task)
{
	
}

int dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpu)
{
	int fpvalid = 0;

#if defined(CONFIG_SH_FPU)
	struct task_struct *tsk = current;

	fpvalid = !!tsk_used_math(tsk);
	if (fpvalid)
		fpvalid = !fpregs_get(tsk, NULL, 0,
				      sizeof(struct user_fpu_struct),
				      fpu, NULL);
#endif

	return fpvalid;
}
EXPORT_SYMBOL(dump_fpu);

void prepare_to_copy(struct task_struct *tsk)
{
	unlazy_fpu(tsk, task_pt_regs(tsk));
}

asmlinkage void ret_from_fork(void);

int copy_thread(unsigned long clone_flags, unsigned long usp,
		unsigned long unused,
		struct task_struct *p, struct pt_regs *regs)
{
	struct thread_info *ti = task_thread_info(p);
	struct pt_regs *childregs;

#if defined(CONFIG_SH_DSP)
	struct task_struct *tsk = current;

	if (is_dsp_enabled(tsk)) {
		p->thread.dsp_status = tsk->thread.dsp_status;
	}
#endif

	childregs = task_pt_regs(p);
	*childregs = *regs;

	if (user_mode(regs)) {
		childregs->regs[15] = usp;
		ti->addr_limit = USER_DS;
	} else {
		childregs->regs[15] = (unsigned long)childregs;
		ti->addr_limit = KERNEL_DS;
		ti->status &= ~TS_USEDFPU;
		p->fpu_counter = 0;
	}

	if (clone_flags & CLONE_SETTLS)
		childregs->gbr = childregs->regs[0];

	childregs->regs[0] = 0; 

	p->thread.sp = (unsigned long) childregs;
	p->thread.pc = (unsigned long) ret_from_fork;

	memset(p->thread.ptrace_bps, 0, sizeof(p->thread.ptrace_bps));

	return 0;
}

__notrace_funcgraph struct task_struct *
__switch_to(struct task_struct *prev, struct task_struct *next)
{
	struct thread_struct *next_t = &next->thread;

	unlazy_fpu(prev, task_pt_regs(prev));

	
	if (next->fpu_counter > 5)
		prefetch(next_t->xstate);

#ifdef CONFIG_MMU
	asm volatile("ldc	%0, r7_bank"
		     : 
		     : "r" (task_thread_info(next)));
#endif

	if (next->fpu_counter > 5)
		__fpu_state_restore();

	return prev;
}

asmlinkage int sys_fork(unsigned long r4, unsigned long r5,
			unsigned long r6, unsigned long r7,
			struct pt_regs __regs)
{
#ifdef CONFIG_MMU
	struct pt_regs *regs = RELOC_HIDE(&__regs, 0);
	return do_fork(SIGCHLD, regs->regs[15], regs, 0, NULL, NULL);
#else
	
	return -EINVAL;
#endif
}

asmlinkage int sys_clone(unsigned long clone_flags, unsigned long newsp,
			 unsigned long parent_tidptr,
			 unsigned long child_tidptr,
			 struct pt_regs __regs)
{
	struct pt_regs *regs = RELOC_HIDE(&__regs, 0);
	if (!newsp)
		newsp = regs->regs[15];
	return do_fork(clone_flags, newsp, regs, 0,
			(int __user *)parent_tidptr,
			(int __user *)child_tidptr);
}

asmlinkage int sys_vfork(unsigned long r4, unsigned long r5,
			 unsigned long r6, unsigned long r7,
			 struct pt_regs __regs)
{
	struct pt_regs *regs = RELOC_HIDE(&__regs, 0);
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs->regs[15], regs,
		       0, NULL, NULL);
}

asmlinkage int sys_execve(const char __user *ufilename,
			  const char __user *const __user *uargv,
			  const char __user *const __user *uenvp,
			  unsigned long r7, struct pt_regs __regs)
{
	struct pt_regs *regs = RELOC_HIDE(&__regs, 0);
	int error;
	char *filename;

	filename = getname(ufilename);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;

	error = do_execve(filename, uargv, uenvp, regs);
	putname(filename);
out:
	return error;
}

unsigned long get_wchan(struct task_struct *p)
{
	unsigned long pc;

	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	pc = thread_saved_pc(p);

#ifdef CONFIG_FRAME_POINTER
	if (in_sched_functions(pc)) {
		unsigned long schedule_frame = (unsigned long)p->thread.sp;
		return ((unsigned long *)schedule_frame)[21];
	}
#endif

	return pc;
}
