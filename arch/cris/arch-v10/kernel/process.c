/*
 *  linux/arch/cris/kernel/process.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Copyright (C) 2000-2002  Axis Communications AB
 *
 *  Authors:   Bjorn Wesen (bjornw@axis.com)
 *             Mikael Starvik (starvik@axis.com)
 *
 * This file handles the architecture-dependent parts of process handling..
 */

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <arch/svinto.h>
#include <linux/init.h>
#include <arch/system.h>

#ifdef CONFIG_ETRAX_GPIO
void etrax_gpio_wake_up_check(void); 
#endif

void default_idle(void)
{
#ifdef CONFIG_ETRAX_GPIO
  etrax_gpio_wake_up_check();
#endif
}


void exit_thread(void)
{
	
}


void hard_reset_now (void)
{
#if defined(CONFIG_ETRAX_WATCHDOG) && !defined(CONFIG_SVINTO_SIM)
	extern int cause_of_death;
#endif

	printk("*** HARD RESET ***\n");
	local_irq_disable();

#if defined(CONFIG_ETRAX_WATCHDOG) && !defined(CONFIG_SVINTO_SIM)
	cause_of_death = 0xbedead;
#else
	*R_WATCHDOG = IO_FIELD(R_WATCHDOG, key, 3) |
		IO_STATE(R_WATCHDOG, enable, start);
#endif

	while(1)  ;
}

unsigned long thread_saved_pc(struct task_struct *t)
{
	return task_pt_regs(t)->irp;
}

static void kernel_thread_helper(void* dummy, int (*fn)(void *), void * arg)
{
  fn(arg);
  do_exit(-1); 
}

int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	struct pt_regs regs;

	memset(&regs, 0, sizeof(regs));

        
	regs.r11 = (unsigned long)fn;
	regs.r12 = (unsigned long)arg;
	regs.irp = (unsigned long)kernel_thread_helper;
	regs.dccr = 1 << I_DCCR_BITNR;

	
        return do_fork(flags | CLONE_VM | CLONE_UNTRACED, 0, &regs, 0, NULL, NULL);
}

asmlinkage void ret_from_fork(void);

int copy_thread(unsigned long clone_flags, unsigned long usp,
		unsigned long unused,
		struct task_struct *p, struct pt_regs *regs)
{
	struct pt_regs * childregs;
	struct switch_stack *swstack;
	

	childregs = task_pt_regs(p);
        
	*childregs = *regs;  
        
        p->set_child_tid = p->clear_child_tid = NULL;

        childregs->r10 = 0;  
	
	

	swstack = ((struct switch_stack *)childregs) - 1;

	swstack->r9 = 0; 

	

	swstack->return_ip = (unsigned long) ret_from_fork; 
	
	

	p->thread.usp = usp;	

	

	p->thread.ksp = (unsigned long) swstack;

#ifdef DEBUG
	printk("copy_thread: new regs at 0x%p, as shown below:\n", childregs);
	show_registers(childregs);
#endif

	return 0;
}


asmlinkage int sys_fork(long r10, long r11, long r12, long r13, long mof, long srp,
			struct pt_regs *regs)
{
	return do_fork(SIGCHLD, rdusp(), regs, 0, NULL, NULL);
}

asmlinkage int sys_clone(unsigned long newusp, unsigned long flags,
			 int* parent_tid, int* child_tid, long mof, long srp,
			 struct pt_regs *regs)
{
	if (!newusp)
		newusp = rdusp();
	return do_fork(flags, newusp, regs, 0, parent_tid, child_tid);
}


asmlinkage int sys_vfork(long r10, long r11, long r12, long r13, long mof, long srp,
			 struct pt_regs *regs)
{
        return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, rdusp(), regs, 0, NULL, NULL);
}

asmlinkage int sys_execve(const char *fname,
			  const char *const *argv,
			  const char *const *envp,
			  long r13, long mof, long srp, 
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

unsigned long get_wchan(struct task_struct *p)
{
#if 0
	

        unsigned long ebp, esp, eip;
        unsigned long stack_page;
        int count = 0;
        if (!p || p == current || p->state == TASK_RUNNING)
                return 0;
        stack_page = (unsigned long)p;
        esp = p->thread.esp;
        if (!stack_page || esp < stack_page || esp > 8188+stack_page)
                return 0;
        
        ebp = *(unsigned long *) esp;
        do {
                if (ebp < stack_page || ebp > 8184+stack_page)
                        return 0;
                eip = *(unsigned long *) (ebp+4);
		if (!in_sched_functions(eip))
			return eip;
                ebp = *(unsigned long *) ebp;
        } while (count++ < 16);
#endif
        return 0;
}
#undef last_sched
#undef first_sched

void show_regs(struct pt_regs * regs)
{
	unsigned long usp = rdusp();
	printk("IRP: %08lx SRP: %08lx DCCR: %08lx USP: %08lx MOF: %08lx\n",
	       regs->irp, regs->srp, regs->dccr, usp, regs->mof );
	printk(" r0: %08lx  r1: %08lx   r2: %08lx  r3: %08lx\n",
	       regs->r0, regs->r1, regs->r2, regs->r3);
	printk(" r4: %08lx  r5: %08lx   r6: %08lx  r7: %08lx\n",
	       regs->r4, regs->r5, regs->r6, regs->r7);
	printk(" r8: %08lx  r9: %08lx  r10: %08lx r11: %08lx\n",
	       regs->r8, regs->r9, regs->r10, regs->r11);
	printk("r12: %08lx r13: %08lx oR10: %08lx\n",
	       regs->r12, regs->r13, regs->orig_r10);
}

