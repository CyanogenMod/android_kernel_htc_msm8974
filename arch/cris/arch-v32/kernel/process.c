/*
 *  Copyright (C) 2000-2003  Axis Communications AB
 *
 *  Authors:   Bjorn Wesen (bjornw@axis.com)
 *             Mikael Starvik (starvik@axis.com)
 *             Tobias Anderberg (tobiasa@axis.com), CRISv32 port.
 *
 * This file handles the architecture-dependent parts of process handling..
 */

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <hwregs/reg_rdwr.h>
#include <hwregs/reg_map.h>
#include <hwregs/timer_defs.h>
#include <hwregs/intr_vect_defs.h>

extern void stop_watchdog(void);

extern int cris_hlt_counter;

void default_idle(void)
{
	local_irq_disable();
	if (!need_resched() && !cris_hlt_counter) {
	        
		__asm__ volatile("ei    \n\t"
                                 "halt      ");
	}
	local_irq_enable();
}


extern void deconfigure_bp(long pid);
void exit_thread(void)
{
	deconfigure_bp(current->pid);
}

extern void arch_enable_nmi(void);

void
hard_reset_now(void)
{
#if defined(CONFIG_ETRAX_WATCHDOG)
	extern int cause_of_death;
#endif

	printk("*** HARD RESET ***\n");
	local_irq_disable();

#if defined(CONFIG_ETRAX_WATCHDOG)
	cause_of_death = 0xbedead;
#else
{
	reg_timer_rw_wd_ctrl wd_ctrl = {0};

	stop_watchdog();

	wd_ctrl.key = 16;	
	wd_ctrl.cnt = 1;	
	wd_ctrl.cmd = regk_timer_start;

        arch_enable_nmi();
	REG_WR(timer, regi_timer0, rw_wd_ctrl, wd_ctrl);
}
#endif

	while (1)
		; 
}

unsigned long thread_saved_pc(struct task_struct *t)
{
	return task_pt_regs(t)->erp;
}

static void
kernel_thread_helper(void* dummy, int (*fn)(void *), void * arg)
{
	fn(arg);
	do_exit(-1); 
}

int
kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	struct pt_regs regs;

	memset(&regs, 0, sizeof(regs));

        
	regs.r11 = (unsigned long) fn;
	regs.r12 = (unsigned long) arg;
	regs.erp = (unsigned long) kernel_thread_helper;
	regs.ccs = 1 << (I_CCS_BITNR + CCS_SHIFT);

	
        return do_fork(flags | CLONE_VM | CLONE_UNTRACED, 0, &regs, 0, NULL, NULL);
}


extern asmlinkage void ret_from_fork(void);

int
copy_thread(unsigned long clone_flags, unsigned long usp,
	unsigned long unused,
	struct task_struct *p, struct pt_regs *regs)
{
	struct pt_regs *childregs;
	struct switch_stack *swstack;

	childregs = task_pt_regs(p);
	*childregs = *regs;	
        p->set_child_tid = p->clear_child_tid = NULL;
        childregs->r10 = 0;	

	if (p->mm && (clone_flags & CLONE_SETTLS)) {
		task_thread_info(p)->tls = regs->mof;
	}

	
	swstack = ((struct switch_stack *) childregs) - 1;

	
	swstack->r9 = 0;

	swstack->return_ip = (unsigned long) ret_from_fork;

	
	p->thread.usp = usp;
	p->thread.ksp = (unsigned long) swstack;

	return 0;
}

asmlinkage int
sys_fork(long r10, long r11, long r12, long r13, long mof, long srp,
	struct pt_regs *regs)
{
	return do_fork(SIGCHLD, rdusp(), regs, 0, NULL, NULL);
}

asmlinkage int
sys_clone(unsigned long newusp, unsigned long flags, int *parent_tid, int *child_tid,
	unsigned long tls, long srp, struct pt_regs *regs)
{
	if (!newusp)
		newusp = rdusp();

	return do_fork(flags, newusp, regs, 0, parent_tid, child_tid);
}

asmlinkage int
sys_vfork(long r10, long r11, long r12, long r13, long mof, long srp,
	struct pt_regs *regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, rdusp(), regs, 0, NULL, NULL);
}

asmlinkage int
sys_execve(const char *fname,
	   const char *const *argv,
	   const char *const *envp, long r13, long mof, long srp,
	   struct pt_regs *regs)
{
	int error;
	char *filename;

	filename = getname(fname);
	error = PTR_ERR(filename);

	if (IS_ERR(filename))
	        goto out;

	error = do_execve(filename, argv, envp, regs);
	putname(filename);
 out:
	return error;
}

unsigned long
get_wchan(struct task_struct *p)
{
	
	return 0;
}
#undef last_sched
#undef first_sched

void show_regs(struct pt_regs * regs)
{
	unsigned long usp = rdusp();
        printk("ERP: %08lx SRP: %08lx  CCS: %08lx USP: %08lx MOF: %08lx\n",
		regs->erp, regs->srp, regs->ccs, usp, regs->mof);

	printk(" r0: %08lx  r1: %08lx   r2: %08lx  r3: %08lx\n",
		regs->r0, regs->r1, regs->r2, regs->r3);

	printk(" r4: %08lx  r5: %08lx   r6: %08lx  r7: %08lx\n",
		regs->r4, regs->r5, regs->r6, regs->r7);

	printk(" r8: %08lx  r9: %08lx  r10: %08lx r11: %08lx\n",
		regs->r8, regs->r9, regs->r10, regs->r11);

	printk("r12: %08lx r13: %08lx oR10: %08lx\n",
		regs->r12, regs->r13, regs->orig_r10);
}
