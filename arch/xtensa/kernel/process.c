/*
 * arch/xtensa/kernel/process.c
 *
 * Xtensa Processor version.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 *
 * Joe Taylor <joe@tensilica.com, joetylr@yahoo.com>
 * Chris Zankel <chris@zankel.net>
 * Marc Gauthier <marc@tensilica.com, marc@alumni.uwaterloo.ca>
 * Kevin Chea
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/elf.h>
#include <linux/init.h>
#include <linux/prctl.h>
#include <linux/init_task.h>
#include <linux/module.h>
#include <linux/mqueue.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/platform.h>
#include <asm/mmu.h>
#include <asm/irq.h>
#include <linux/atomic.h>
#include <asm/asm-offsets.h>
#include <asm/regs.h>

extern void ret_from_fork(void);

struct task_struct *current_set[NR_CPUS] = {&init_task, };

void (*pm_power_off)(void) = NULL;
EXPORT_SYMBOL(pm_power_off);


#if XTENSA_HAVE_COPROCESSORS

void coprocessor_release_all(struct thread_info *ti)
{
	unsigned long cpenable;
	int i;

	

	preempt_disable();

	

	cpenable = ti->cpenable;

	for (i = 0; i < XCHAL_CP_MAX; i++) {
		if (coprocessor_owner[i] == ti) {
			coprocessor_owner[i] = 0;
			cpenable &= ~(1 << i);
		}
	}

	ti->cpenable = cpenable;
	coprocessor_clear_cpenable();

	preempt_enable();
}

void coprocessor_flush_all(struct thread_info *ti)
{
	unsigned long cpenable;
	int i;

	preempt_disable();

	cpenable = ti->cpenable;

	for (i = 0; i < XCHAL_CP_MAX; i++) {
		if ((cpenable & 1) != 0 && coprocessor_owner[i] == ti)
			coprocessor_flush(ti, i);
		cpenable >>= 1;
	}

	preempt_enable();
}

#endif



void cpu_idle(void)
{
  	local_irq_enable();

	
	while (1) {
		while (!need_resched())
			platform_idle();
		schedule_preempt_disabled();
	}
}

void exit_thread(void)
{
#if XTENSA_HAVE_COPROCESSORS
	coprocessor_release_all(current_thread_info());
#endif
}

void flush_thread(void)
{
#if XTENSA_HAVE_COPROCESSORS
	struct thread_info *ti = current_thread_info();
	coprocessor_flush_all(ti);
	coprocessor_release_all(ti);
#endif
}

void prepare_to_copy(struct task_struct *tsk)
{
#if XTENSA_HAVE_COPROCESSORS
	coprocessor_flush_all(task_thread_info(tsk));
#endif
}


int copy_thread(unsigned long clone_flags, unsigned long usp,
		unsigned long unused,
                struct task_struct * p, struct pt_regs * regs)
{
	struct pt_regs *childregs;
	struct thread_info *ti;
	unsigned long tos;
	int user_mode = user_mode(regs);

	
	tos = (unsigned long)task_stack_page(p) + THREAD_SIZE;
	if (user_mode)
		childregs = (struct pt_regs*)(tos - PT_USER_SIZE);
	else
		childregs = (struct pt_regs*)tos - 1;

	*childregs = *regs;

	
	*((int*)childregs - 3) = (unsigned long)childregs;
	*((int*)childregs - 4) = 0;

	childregs->areg[1] = tos;
	childregs->areg[2] = 0;
	p->set_child_tid = p->clear_child_tid = NULL;
	p->thread.ra = MAKE_RA_FOR_CALL((unsigned long)ret_from_fork, 0x1);
	p->thread.sp = (unsigned long)childregs;

	if (user_mode(regs)) {

		int len = childregs->wmask & ~0xf;
		childregs->areg[1] = usp;
		memcpy(&childregs->areg[XCHAL_NUM_AREGS - len/4],
		       &regs->areg[XCHAL_NUM_AREGS - len/4], len);
		if (clone_flags & CLONE_SETTLS)
			childregs->areg[2] = childregs->areg[6];

	} else {
		
		childregs->wmask = 1;
	}

#if (XTENSA_HAVE_COPROCESSORS || XTENSA_HAVE_IO_PORTS)
	ti = task_thread_info(p);
	ti->cpenable = 0;
#endif

	return 0;
}



unsigned long get_wchan(struct task_struct *p)
{
	unsigned long sp, pc;
	unsigned long stack_page = (unsigned long) task_stack_page(p);
	int count = 0;

	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	sp = p->thread.sp;
	pc = MAKE_PC_FROM_RA(p->thread.ra, p->thread.sp);

	do {
		if (sp < stack_page + sizeof(struct task_struct) ||
		    sp >= (stack_page + THREAD_SIZE) ||
		    pc == 0)
			return 0;
		if (!in_sched_functions(pc))
			return pc;

		

		pc = MAKE_PC_FROM_RA(*(unsigned long*)sp - 4, sp);
		sp = *(unsigned long *)sp - 3;
	} while (count++ < 16);
	return 0;
}


void xtensa_elf_core_copy_regs (xtensa_gregset_t *elfregs, struct pt_regs *regs)
{
	unsigned long wb, ws, wm;
	int live, last;

	wb = regs->windowbase;
	ws = regs->windowstart;
	wm = regs->wmask;
	ws = ((ws >> wb) | (ws << (WSBITS - wb))) & ((1 << WSBITS) - 1);

	

	memset(elfregs, 0, sizeof (elfregs));


	elfregs->pc		= regs->pc;
	elfregs->ps		= (regs->ps & ~(1 << PS_EXCM_BIT));
	elfregs->lbeg		= regs->lbeg;
	elfregs->lend		= regs->lend;
	elfregs->lcount		= regs->lcount;
	elfregs->sar		= regs->sar;
	elfregs->windowstart	= ws;

	live = (wm & 2) ? 4 : (wm & 4) ? 8 : (wm & 8) ? 12 : 16;
	last = XCHAL_NUM_AREGS - (wm >> 4) * 4;
	memcpy(elfregs->a, regs->areg, live * 4);
	memcpy(elfregs->a + last, regs->areg + last, (wm >> 4) * 16);
}

int dump_fpu(void)
{
	return 0;
}

asmlinkage
long xtensa_clone(unsigned long clone_flags, unsigned long newsp,
                  void __user *parent_tid, void *child_tls,
                  void __user *child_tid, long a5,
                  struct pt_regs *regs)
{
        if (!newsp)
                newsp = regs->areg[1];
        return do_fork(clone_flags, newsp, regs, 0, parent_tid, child_tid);
}


asmlinkage
long xtensa_execve(const char __user *name,
		   const char __user *const __user *argv,
                   const char __user *const __user *envp,
                   long a3, long a4, long a5,
                   struct pt_regs *regs)
{
	long error;
	char * filename;

	filename = getname(name);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve(filename, argv, envp, regs);
	putname(filename);
out:
	return error;
}

