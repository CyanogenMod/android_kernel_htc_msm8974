/*
 * Process creation support for Hexagon
 *
 * Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/tick.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

static void __noreturn kernel_thread_helper(void *arg, int (*fn)(void *))
{
	do_exit(fn(arg));
}

int kernel_thread(int (*fn)(void *), void *arg, unsigned long flags)
{
	struct pt_regs regs;

	memset(&regs, 0, sizeof(regs));
	regs.r00 = (unsigned long) arg;
	regs.r01 = (unsigned long) fn;
	pt_set_elr(&regs, (unsigned long)kernel_thread_helper);
	pt_set_kmode(&regs);

	return do_fork(flags|CLONE_VM|CLONE_UNTRACED, 0, &regs, 0, NULL, NULL);
}
EXPORT_SYMBOL(kernel_thread);

void start_thread(struct pt_regs *regs, unsigned long pc, unsigned long sp)
{
	
	set_fs(USER_DS);
	
	memset(regs, 0, sizeof(*regs));
	
	pt_set_usermode(regs);
	pt_set_elr(regs, pc);
	pt_set_rte_sp(regs, sp);
}

static void default_idle(void)
{
	__vmwait();
}

void (*idle_sleep)(void) = default_idle;

void cpu_idle(void)
{
	while (1) {
		tick_nohz_idle_enter();
		local_irq_disable();
		while (!need_resched()) {
			idle_sleep();
			
			local_irq_enable();	
			local_irq_disable();
		}
		local_irq_enable();
		tick_nohz_idle_exit();
		schedule();
	}
}

unsigned long thread_saved_pc(struct task_struct *tsk)
{
	return 0;
}

int copy_thread(unsigned long clone_flags, unsigned long usp,
		unsigned long unused, struct task_struct *p,
		struct pt_regs *regs)
{
	struct thread_info *ti = task_thread_info(p);
	struct hexagon_switch_stack *ss;
	struct pt_regs *childregs;
	asmlinkage void ret_from_fork(void);

	childregs = (struct pt_regs *) (((unsigned long) ti + THREAD_SIZE) -
					sizeof(*childregs));

	memcpy(childregs, regs, sizeof(*childregs));
	ti->regs = childregs;

	ss = (struct hexagon_switch_stack *) ((unsigned long) childregs -
						    sizeof(*ss));
	ss->lr = (unsigned long)ret_from_fork;
	p->thread.switch_sp = ss;

	
	if (user_mode(childregs)) {
		pt_set_rte_sp(childregs, usp);

		
		childregs->r00 = 0;

		if (clone_flags & CLONE_SETTLS)
			childregs->ugp = childregs->r04;

	} else {
		pt_set_rte_sp(childregs, (unsigned long)(childregs + 1));
		childregs->THREADINFO_REG = (unsigned long) ti;
	}

	p->stack = (void *)ti;

	return 0;
}

void release_thread(struct task_struct *dead_task)
{
}

void exit_thread(void)
{
}

void flush_thread(void)
{
}

unsigned long get_wchan(struct task_struct *p)
{
	unsigned long fp, pc;
	unsigned long stack_page;
	int count = 0;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	stack_page = (unsigned long)task_stack_page(p);
	fp = ((struct hexagon_switch_stack *)p->thread.switch_sp)->fp;
	do {
		if (fp < (stack_page + sizeof(struct thread_info)) ||
			fp >= (THREAD_SIZE - 8 + stack_page))
			return 0;
		pc = ((unsigned long *)fp)[1];
		if (!in_sched_functions(pc))
			return pc;
		fp = *(unsigned long *) fp;
	} while (count++ < 16);

	return 0;
}

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

int dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpu)
{
	return 0;
}
