/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#include <linux/sched.h>
#include <linux/preempt.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/elfcore.h>
#include <linux/tick.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/compat.h>
#include <linux/hardirq.h>
#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/tracehook.h>
#include <linux/signal.h>
#include <asm/stack.h>
#include <asm/switch_to.h>
#include <asm/homecache.h>
#include <asm/syscalls.h>
#include <asm/traps.h>
#include <asm/setup.h>
#ifdef CONFIG_HARDWALL
#include <asm/hardwall.h>
#endif
#include <arch/chip.h>
#include <arch/abi.h>
#include <arch/sim_def.h>


static int no_idle_nap;
static int __init idle_setup(char *str)
{
	if (!str)
		return -EINVAL;

	if (!strcmp(str, "poll")) {
		pr_info("using polling idle threads.\n");
		no_idle_nap = 1;
	} else if (!strcmp(str, "halt"))
		no_idle_nap = 0;
	else
		return -1;

	return 0;
}
early_param("idle", idle_setup);

void cpu_idle(void)
{
	int cpu = smp_processor_id();


	current_thread_info()->status |= TS_POLLING;

	if (no_idle_nap) {
		while (1) {
			while (!need_resched())
				cpu_relax();
			schedule();
		}
	}

	
	while (1) {
		tick_nohz_idle_enter();
		rcu_idle_enter();
		while (!need_resched()) {
			if (cpu_is_offline(cpu))
				BUG();  

			local_irq_disable();
			__get_cpu_var(irq_stat).idle_timestamp = jiffies;
			current_thread_info()->status &= ~TS_POLLING;
			smp_mb();

			if (!need_resched())
				_cpu_idle();
			else
				local_irq_enable();
			current_thread_info()->status |= TS_POLLING;
		}
		rcu_idle_exit();
		tick_nohz_idle_exit();
		schedule_preempt_disabled();
	}
}

struct thread_info *alloc_thread_info_node(struct task_struct *task, int node)
{
	struct page *page;
	gfp_t flags = GFP_KERNEL;

#ifdef CONFIG_DEBUG_STACK_USAGE
	flags |= __GFP_ZERO;
#endif

	page = alloc_pages_node(node, flags, THREAD_SIZE_ORDER);
	if (!page)
		return NULL;

	return (struct thread_info *)page_address(page);
}

void free_thread_info(struct thread_info *info)
{
	struct single_step_state *step_state = info->step_state;

#ifdef CONFIG_HARDWALL
	if (info->task->thread.hardwall)
		hardwall_deactivate(info->task);
#endif

	if (step_state) {

		kfree(step_state);
	}

	free_pages((unsigned long)info, THREAD_SIZE_ORDER);
}

static void save_arch_state(struct thread_struct *t);

int copy_thread(unsigned long clone_flags, unsigned long sp,
		unsigned long stack_size,
		struct task_struct *p, struct pt_regs *regs)
{
	struct pt_regs *childregs;
	unsigned long ksp;

	if (sp == 0 && regs->ex1 == PL_ICS_EX1(KERNEL_PL, 0))
		sp = KSTK_TOP(p);

	task_thread_info(p)->step_state = NULL;

	p->thread.pc = (unsigned long) ret_from_fork;

	
	p->thread.usp0 = sp;

	
	p->thread.creator_pid = current->pid;

	childregs = task_pt_regs(p);
	*childregs = *regs;
	childregs->regs[0] = 0;         
	childregs->sp = sp;  

	if (clone_flags & CLONE_SETTLS)
		childregs->tp = regs->regs[4];

	ksp = (unsigned long) childregs;
	ksp -= C_ABI_SAVE_AREA_SIZE;   
	((long *)ksp)[0] = ((long *)ksp)[1] = 0;
	ksp -= CALLEE_SAVED_REGS_COUNT * sizeof(unsigned long);
	memcpy((void *)ksp, &regs->regs[CALLEE_SAVED_FIRST_REG],
	       CALLEE_SAVED_REGS_COUNT * sizeof(unsigned long));
	ksp -= C_ABI_SAVE_AREA_SIZE;   
	((long *)ksp)[0] = ((long *)ksp)[1] = 0;
	p->thread.ksp = ksp;

#if CHIP_HAS_TILE_DMA()
	memset(&p->thread.tile_dma_state, 0, sizeof(struct tile_dma_state));
	memset(&p->thread.dma_async_tlb, 0, sizeof(struct async_tlb));
#endif

#if CHIP_HAS_SN_PROC()
	
	p->thread.sn_proc_running = 0;
	memset(&p->thread.sn_async_tlb, 0, sizeof(struct async_tlb));
#endif

#if CHIP_HAS_PROC_STATUS_SPR()
	
	p->thread.proc_status = 0;
#endif

#ifdef CONFIG_HARDWALL
	
	p->thread.hardwall = NULL;
#endif


	save_arch_state(&p->thread);

	return 0;
}

struct task_struct *validate_current(void)
{
	static struct task_struct corrupt = { .comm = "<corrupt>" };
	struct task_struct *tsk = current;
	if (unlikely((unsigned long)tsk < PAGE_OFFSET ||
		     (high_memory && (void *)tsk > high_memory) ||
		     ((unsigned long)tsk & (__alignof__(*tsk) - 1)) != 0)) {
		pr_err("Corrupt 'current' %p (sp %#lx)\n", tsk, stack_pointer);
		tsk = &corrupt;
	}
	return tsk;
}

struct task_struct *sim_notify_fork(struct task_struct *prev)
{
	struct task_struct *tsk = current;
	__insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_OS_FORK_PARENT |
		     (tsk->thread.creator_pid << _SIM_CONTROL_OPERATOR_BITS));
	__insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_OS_FORK |
		     (tsk->pid << _SIM_CONTROL_OPERATOR_BITS));
	return prev;
}

int dump_task_regs(struct task_struct *tsk, elf_gregset_t *regs)
{
	struct pt_regs *ptregs = task_pt_regs(tsk);
	elf_core_copy_regs(regs, ptregs);
	return 1;
}

#if CHIP_HAS_TILE_DMA()

void grant_dma_mpls(void)
{
#if CONFIG_KERNEL_PL == 2
	__insn_mtspr(SPR_MPL_DMA_CPL_SET_1, 1);
	__insn_mtspr(SPR_MPL_DMA_NOTIFY_SET_1, 1);
#else
	__insn_mtspr(SPR_MPL_DMA_CPL_SET_0, 1);
	__insn_mtspr(SPR_MPL_DMA_NOTIFY_SET_0, 1);
#endif
}

void restrict_dma_mpls(void)
{
#if CONFIG_KERNEL_PL == 2
	__insn_mtspr(SPR_MPL_DMA_CPL_SET_2, 1);
	__insn_mtspr(SPR_MPL_DMA_NOTIFY_SET_2, 1);
#else
	__insn_mtspr(SPR_MPL_DMA_CPL_SET_1, 1);
	__insn_mtspr(SPR_MPL_DMA_NOTIFY_SET_1, 1);
#endif
}

static void save_tile_dma_state(struct tile_dma_state *dma)
{
	unsigned long state = __insn_mfspr(SPR_DMA_USER_STATUS);
	unsigned long post_suspend_state;

	
	if ((state & DMA_STATUS_MASK) == SPR_DMA_STATUS__RUNNING_MASK)
		__insn_mtspr(SPR_DMA_CTR, SPR_DMA_CTR__SUSPEND_MASK);

	do {
		post_suspend_state = __insn_mfspr(SPR_DMA_USER_STATUS);
	} while (post_suspend_state & SPR_DMA_STATUS__BUSY_MASK);

	dma->src = __insn_mfspr(SPR_DMA_SRC_ADDR);
	dma->src_chunk = __insn_mfspr(SPR_DMA_SRC_CHUNK_ADDR);
	dma->dest = __insn_mfspr(SPR_DMA_DST_ADDR);
	dma->dest_chunk = __insn_mfspr(SPR_DMA_DST_CHUNK_ADDR);
	dma->strides = __insn_mfspr(SPR_DMA_STRIDE);
	dma->chunk_size = __insn_mfspr(SPR_DMA_CHUNK_SIZE);
	dma->byte = __insn_mfspr(SPR_DMA_BYTE);
	dma->status = (state & SPR_DMA_STATUS__RUNNING_MASK) |
		(post_suspend_state & SPR_DMA_STATUS__DONE_MASK);
}

static void restore_tile_dma_state(struct thread_struct *t)
{
	const struct tile_dma_state *dma = &t->tile_dma_state;

	if ((dma->status & SPR_DMA_STATUS__DONE_MASK) &&
	    !(__insn_mfspr(SPR_DMA_USER_STATUS) & SPR_DMA_STATUS__DONE_MASK)) {
		__insn_mtspr(SPR_DMA_BYTE, 0);
		__insn_mtspr(SPR_DMA_CTR, SPR_DMA_CTR__REQUEST_MASK);
		while (__insn_mfspr(SPR_DMA_USER_STATUS) &
		       SPR_DMA_STATUS__BUSY_MASK)
			;
	}

	__insn_mtspr(SPR_DMA_SRC_ADDR, dma->src);
	__insn_mtspr(SPR_DMA_SRC_CHUNK_ADDR, dma->src_chunk);
	__insn_mtspr(SPR_DMA_DST_ADDR, dma->dest);
	__insn_mtspr(SPR_DMA_DST_CHUNK_ADDR, dma->dest_chunk);
	__insn_mtspr(SPR_DMA_STRIDE, dma->strides);
	__insn_mtspr(SPR_DMA_CHUNK_SIZE, dma->chunk_size);
	__insn_mtspr(SPR_DMA_BYTE, dma->byte);

	if ((dma->status & DMA_STATUS_MASK) == SPR_DMA_STATUS__RUNNING_MASK) {
		t->dma_async_tlb.fault_num = 0;
		__insn_mtspr(SPR_DMA_CTR, SPR_DMA_CTR__REQUEST_MASK);
	}
}

#endif

static void save_arch_state(struct thread_struct *t)
{
#if CHIP_HAS_SPLIT_INTR_MASK()
	t->interrupt_mask = __insn_mfspr(SPR_INTERRUPT_MASK_0_0) |
		((u64)__insn_mfspr(SPR_INTERRUPT_MASK_0_1) << 32);
#else
	t->interrupt_mask = __insn_mfspr(SPR_INTERRUPT_MASK_0);
#endif
	t->ex_context[0] = __insn_mfspr(SPR_EX_CONTEXT_0_0);
	t->ex_context[1] = __insn_mfspr(SPR_EX_CONTEXT_0_1);
	t->system_save[0] = __insn_mfspr(SPR_SYSTEM_SAVE_0_0);
	t->system_save[1] = __insn_mfspr(SPR_SYSTEM_SAVE_0_1);
	t->system_save[2] = __insn_mfspr(SPR_SYSTEM_SAVE_0_2);
	t->system_save[3] = __insn_mfspr(SPR_SYSTEM_SAVE_0_3);
	t->intctrl_0 = __insn_mfspr(SPR_INTCTRL_0_STATUS);
#if CHIP_HAS_PROC_STATUS_SPR()
	t->proc_status = __insn_mfspr(SPR_PROC_STATUS);
#endif
#if !CHIP_HAS_FIXED_INTVEC_BASE()
	t->interrupt_vector_base = __insn_mfspr(SPR_INTERRUPT_VECTOR_BASE_0);
#endif
#if CHIP_HAS_TILE_RTF_HWM()
	t->tile_rtf_hwm = __insn_mfspr(SPR_TILE_RTF_HWM);
#endif
#if CHIP_HAS_DSTREAM_PF()
	t->dstream_pf = __insn_mfspr(SPR_DSTREAM_PF);
#endif
}

static void restore_arch_state(const struct thread_struct *t)
{
#if CHIP_HAS_SPLIT_INTR_MASK()
	__insn_mtspr(SPR_INTERRUPT_MASK_0_0, (u32) t->interrupt_mask);
	__insn_mtspr(SPR_INTERRUPT_MASK_0_1, t->interrupt_mask >> 32);
#else
	__insn_mtspr(SPR_INTERRUPT_MASK_0, t->interrupt_mask);
#endif
	__insn_mtspr(SPR_EX_CONTEXT_0_0, t->ex_context[0]);
	__insn_mtspr(SPR_EX_CONTEXT_0_1, t->ex_context[1]);
	__insn_mtspr(SPR_SYSTEM_SAVE_0_0, t->system_save[0]);
	__insn_mtspr(SPR_SYSTEM_SAVE_0_1, t->system_save[1]);
	__insn_mtspr(SPR_SYSTEM_SAVE_0_2, t->system_save[2]);
	__insn_mtspr(SPR_SYSTEM_SAVE_0_3, t->system_save[3]);
	__insn_mtspr(SPR_INTCTRL_0_STATUS, t->intctrl_0);
#if CHIP_HAS_PROC_STATUS_SPR()
	__insn_mtspr(SPR_PROC_STATUS, t->proc_status);
#endif
#if !CHIP_HAS_FIXED_INTVEC_BASE()
	__insn_mtspr(SPR_INTERRUPT_VECTOR_BASE_0, t->interrupt_vector_base);
#endif
#if CHIP_HAS_TILE_RTF_HWM()
	__insn_mtspr(SPR_TILE_RTF_HWM, t->tile_rtf_hwm);
#endif
#if CHIP_HAS_DSTREAM_PF()
	__insn_mtspr(SPR_DSTREAM_PF, t->dstream_pf);
#endif
}


void _prepare_arch_switch(struct task_struct *next)
{
#if CHIP_HAS_SN_PROC()
	int snctl;
#endif
#if CHIP_HAS_TILE_DMA()
	struct tile_dma_state *dma = &current->thread.tile_dma_state;
	if (dma->enabled)
		save_tile_dma_state(dma);
#endif
#if CHIP_HAS_SN_PROC()
	snctl = __insn_mfspr(SPR_SNCTL);
	current->thread.sn_proc_running =
		(snctl & SPR_SNCTL__FRZPROC_MASK) == 0;
	if (current->thread.sn_proc_running)
		__insn_mtspr(SPR_SNCTL, snctl | SPR_SNCTL__FRZPROC_MASK);
#endif
}


struct task_struct *__sched _switch_to(struct task_struct *prev,
				       struct task_struct *next)
{
	
	save_arch_state(&prev->thread);

#if CHIP_HAS_TILE_DMA()
	if (next->thread.tile_dma_state.enabled) {
		restore_tile_dma_state(&next->thread);
		grant_dma_mpls();
	} else {
		restrict_dma_mpls();
	}
#endif

	
	restore_arch_state(&next->thread);

#if CHIP_HAS_SN_PROC()
	if (next->thread.sn_proc_running) {
		int snctl = __insn_mfspr(SPR_SNCTL);
		__insn_mtspr(SPR_SNCTL, snctl & ~SPR_SNCTL__FRZPROC_MASK);
	}
#endif

#ifdef CONFIG_HARDWALL
	
	if (prev->thread.hardwall != NULL) {
		if (next->thread.hardwall == NULL)
			restrict_network_mpls();
	} else if (next->thread.hardwall != NULL) {
		grant_network_mpls();
	}
#endif

	return __switch_to(prev, next, next_current_ksp0(next));
}

int do_work_pending(struct pt_regs *regs, u32 thread_info_flags)
{
	
	if (!user_mode(regs))
		return 0;

	if (thread_info_flags & _TIF_NEED_RESCHED) {
		schedule();
		return 1;
	}
#if CHIP_HAS_TILE_DMA() || CHIP_HAS_SN_PROC()
	if (thread_info_flags & _TIF_ASYNC_TLB) {
		do_async_page_fault(regs);
		return 1;
	}
#endif
	if (thread_info_flags & _TIF_SIGPENDING) {
		do_signal(regs);
		return 1;
	}
	if (thread_info_flags & _TIF_NOTIFY_RESUME) {
		clear_thread_flag(TIF_NOTIFY_RESUME);
		tracehook_notify_resume(regs);
		if (current->replacement_session_keyring)
			key_replace_session_keyring();
		return 1;
	}
	if (thread_info_flags & _TIF_SINGLESTEP) {
		single_step_once(regs);
		return 0;
	}
	panic("work_pending: bad flags %#x\n", thread_info_flags);
}

SYSCALL_DEFINE5(clone, unsigned long, clone_flags, unsigned long, newsp,
		void __user *, parent_tidptr, void __user *, child_tidptr,
		struct pt_regs *, regs)
{
	if (!newsp)
		newsp = regs->sp;
	return do_fork(clone_flags, newsp, regs, 0,
		       parent_tidptr, child_tidptr);
}

SYSCALL_DEFINE4(execve, const char __user *, path,
		const char __user *const __user *, argv,
		const char __user *const __user *, envp,
		struct pt_regs *, regs)
{
	long error;
	char *filename;

	filename = getname(path);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve(filename, argv, envp, regs);
	putname(filename);
	if (error == 0)
		single_step_execve();
out:
	return error;
}

#ifdef CONFIG_COMPAT
long compat_sys_execve(const char __user *path,
		       compat_uptr_t __user *argv,
		       compat_uptr_t __user *envp,
		       struct pt_regs *regs)
{
	long error;
	char *filename;

	filename = getname(path);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = compat_do_execve(filename, argv, envp, regs);
	putname(filename);
	if (error == 0)
		single_step_execve();
out:
	return error;
}
#endif

unsigned long get_wchan(struct task_struct *p)
{
	struct KBacktraceIterator kbt;

	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	for (KBacktraceIterator_init(&kbt, p, NULL);
	     !KBacktraceIterator_end(&kbt);
	     KBacktraceIterator_next(&kbt)) {
		if (!in_sched_functions(kbt.it.pc))
			return kbt.it.pc;
	}

	return 0;
}

static void start_kernel_thread(int dummy, int (*fn)(int), int arg)
{
	do_exit(fn(arg));
}

int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	struct pt_regs regs;

	memset(&regs, 0, sizeof(regs));
	regs.ex1 = PL_ICS_EX1(KERNEL_PL, 0);  
	regs.pc = (long) start_kernel_thread;
	regs.flags = PT_FLAGS_CALLER_SAVES;   
	regs.regs[1] = (long) fn;             
	regs.regs[2] = (long) arg;            

	
	return do_fork(flags | CLONE_VM | CLONE_UNTRACED, 0, &regs,
		       0, NULL, NULL);
}
EXPORT_SYMBOL(kernel_thread);

void flush_thread(void)
{
	
}

void exit_thread(void)
{
	
}

void show_regs(struct pt_regs *regs)
{
	struct task_struct *tsk = validate_current();
	int i;

	pr_err("\n");
	pr_err(" Pid: %d, comm: %20s, CPU: %d\n",
	       tsk->pid, tsk->comm, smp_processor_id());
#ifdef __tilegx__
	for (i = 0; i < 51; i += 3)
		pr_err(" r%-2d: "REGFMT" r%-2d: "REGFMT" r%-2d: "REGFMT"\n",
		       i, regs->regs[i], i+1, regs->regs[i+1],
		       i+2, regs->regs[i+2]);
	pr_err(" r51: "REGFMT" r52: "REGFMT" tp : "REGFMT"\n",
	       regs->regs[51], regs->regs[52], regs->tp);
	pr_err(" sp : "REGFMT" lr : "REGFMT"\n", regs->sp, regs->lr);
#else
	for (i = 0; i < 52; i += 4)
		pr_err(" r%-2d: "REGFMT" r%-2d: "REGFMT
		       " r%-2d: "REGFMT" r%-2d: "REGFMT"\n",
		       i, regs->regs[i], i+1, regs->regs[i+1],
		       i+2, regs->regs[i+2], i+3, regs->regs[i+3]);
	pr_err(" r52: "REGFMT" tp : "REGFMT" sp : "REGFMT" lr : "REGFMT"\n",
	       regs->regs[52], regs->tp, regs->sp, regs->lr);
#endif
	pr_err(" pc : "REGFMT" ex1: %ld     faultnum: %ld\n",
	       regs->pc, regs->ex1, regs->faultnum);

	dump_stack_regs(regs);
}
