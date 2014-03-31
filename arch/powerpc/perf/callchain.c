/*
 * Performance counter callchain support - powerpc architecture code
 *
 * Copyright © 2009 Paul Mackerras, IBM Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/perf_event.h>
#include <linux/percpu.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <asm/ptrace.h>
#include <asm/pgtable.h>
#include <asm/sigcontext.h>
#include <asm/ucontext.h>
#include <asm/vdso.h>
#ifdef CONFIG_PPC64
#include "../kernel/ppc32.h"
#endif


static int valid_next_sp(unsigned long sp, unsigned long prev_sp)
{
	if (sp & 0xf)
		return 0;		
	if (!validate_sp(sp, current, STACK_FRAME_OVERHEAD))
		return 0;
	if (sp >= prev_sp + STACK_FRAME_OVERHEAD)
		return 1;
	if ((sp & ~(THREAD_SIZE - 1)) != (prev_sp & ~(THREAD_SIZE - 1)))
		return 1;
	return 0;
}

void
perf_callchain_kernel(struct perf_callchain_entry *entry, struct pt_regs *regs)
{
	unsigned long sp, next_sp;
	unsigned long next_ip;
	unsigned long lr;
	long level = 0;
	unsigned long *fp;

	lr = regs->link;
	sp = regs->gpr[1];
	perf_callchain_store(entry, regs->nip);

	if (!validate_sp(sp, current, STACK_FRAME_OVERHEAD))
		return;

	for (;;) {
		fp = (unsigned long *) sp;
		next_sp = fp[0];

		if (next_sp == sp + STACK_INT_FRAME_SIZE &&
		    fp[STACK_FRAME_MARKER] == STACK_FRAME_REGS_MARKER) {
			regs = (struct pt_regs *)(sp + STACK_FRAME_OVERHEAD);
			next_ip = regs->nip;
			lr = regs->link;
			level = 0;
			perf_callchain_store(entry, PERF_CONTEXT_KERNEL);

		} else {
			if (level == 0)
				next_ip = lr;
			else
				next_ip = fp[STACK_FRAME_LR_SAVE];

			if ((level == 1 && next_ip == lr) ||
			    (level <= 1 && !kernel_text_address(next_ip)))
				next_ip = 0;

			++level;
		}

		perf_callchain_store(entry, next_ip);
		if (!valid_next_sp(next_sp, sp))
			return;
		sp = next_sp;
	}
}

#ifdef CONFIG_PPC64
static int read_user_stack_slow(void __user *ptr, void *ret, int nb)
{
	pgd_t *pgdir;
	pte_t *ptep, pte;
	unsigned shift;
	unsigned long addr = (unsigned long) ptr;
	unsigned long offset;
	unsigned long pfn;
	void *kaddr;

	pgdir = current->mm->pgd;
	if (!pgdir)
		return -EFAULT;

	ptep = find_linux_pte_or_hugepte(pgdir, addr, &shift);
	if (!shift)
		shift = PAGE_SHIFT;

	
	offset = addr & ((1UL << shift) - 1);
	addr -= offset;

	if (ptep == NULL)
		return -EFAULT;
	pte = *ptep;
	if (!pte_present(pte) || !(pte_val(pte) & _PAGE_USER))
		return -EFAULT;
	pfn = pte_pfn(pte);
	if (!page_is_ram(pfn))
		return -EFAULT;

	
	kaddr = pfn_to_kaddr(pfn);
	memcpy(ret, kaddr + offset, nb);
	return 0;
}

static int read_user_stack_64(unsigned long __user *ptr, unsigned long *ret)
{
	if ((unsigned long)ptr > TASK_SIZE - sizeof(unsigned long) ||
	    ((unsigned long)ptr & 7))
		return -EFAULT;

	pagefault_disable();
	if (!__get_user_inatomic(*ret, ptr)) {
		pagefault_enable();
		return 0;
	}
	pagefault_enable();

	return read_user_stack_slow(ptr, ret, 8);
}

static int read_user_stack_32(unsigned int __user *ptr, unsigned int *ret)
{
	if ((unsigned long)ptr > TASK_SIZE - sizeof(unsigned int) ||
	    ((unsigned long)ptr & 3))
		return -EFAULT;

	pagefault_disable();
	if (!__get_user_inatomic(*ret, ptr)) {
		pagefault_enable();
		return 0;
	}
	pagefault_enable();

	return read_user_stack_slow(ptr, ret, 4);
}

static inline int valid_user_sp(unsigned long sp, int is_64)
{
	if (!sp || (sp & 7) || sp > (is_64 ? TASK_SIZE : 0x100000000UL) - 32)
		return 0;
	return 1;
}

struct signal_frame_64 {
	char		dummy[__SIGNAL_FRAMESIZE];
	struct ucontext	uc;
	unsigned long	unused[2];
	unsigned int	tramp[6];
	struct siginfo	*pinfo;
	void		*puc;
	struct siginfo	info;
	char		abigap[288];
};

static int is_sigreturn_64_address(unsigned long nip, unsigned long fp)
{
	if (nip == fp + offsetof(struct signal_frame_64, tramp))
		return 1;
	if (vdso64_rt_sigtramp && current->mm->context.vdso_base &&
	    nip == current->mm->context.vdso_base + vdso64_rt_sigtramp)
		return 1;
	return 0;
}

static int sane_signal_64_frame(unsigned long sp)
{
	struct signal_frame_64 __user *sf;
	unsigned long pinfo, puc;

	sf = (struct signal_frame_64 __user *) sp;
	if (read_user_stack_64((unsigned long __user *) &sf->pinfo, &pinfo) ||
	    read_user_stack_64((unsigned long __user *) &sf->puc, &puc))
		return 0;
	return pinfo == (unsigned long) &sf->info &&
		puc == (unsigned long) &sf->uc;
}

static void perf_callchain_user_64(struct perf_callchain_entry *entry,
				   struct pt_regs *regs)
{
	unsigned long sp, next_sp;
	unsigned long next_ip;
	unsigned long lr;
	long level = 0;
	struct signal_frame_64 __user *sigframe;
	unsigned long __user *fp, *uregs;

	next_ip = regs->nip;
	lr = regs->link;
	sp = regs->gpr[1];
	perf_callchain_store(entry, next_ip);

	for (;;) {
		fp = (unsigned long __user *) sp;
		if (!valid_user_sp(sp, 1) || read_user_stack_64(fp, &next_sp))
			return;
		if (level > 0 && read_user_stack_64(&fp[2], &next_ip))
			return;

		if (next_sp - sp >= sizeof(struct signal_frame_64) &&
		    (is_sigreturn_64_address(next_ip, sp) ||
		     (level <= 1 && is_sigreturn_64_address(lr, sp))) &&
		    sane_signal_64_frame(sp)) {
			sigframe = (struct signal_frame_64 __user *) sp;
			uregs = sigframe->uc.uc_mcontext.gp_regs;
			if (read_user_stack_64(&uregs[PT_NIP], &next_ip) ||
			    read_user_stack_64(&uregs[PT_LNK], &lr) ||
			    read_user_stack_64(&uregs[PT_R1], &sp))
				return;
			level = 0;
			perf_callchain_store(entry, PERF_CONTEXT_USER);
			perf_callchain_store(entry, next_ip);
			continue;
		}

		if (level == 0)
			next_ip = lr;
		perf_callchain_store(entry, next_ip);
		++level;
		sp = next_sp;
	}
}

static inline int current_is_64bit(void)
{
	return !test_ti_thread_flag(task_thread_info(current), TIF_32BIT);
}

#else  
static int read_user_stack_32(unsigned int __user *ptr, unsigned int *ret)
{
	int rc;

	if ((unsigned long)ptr > TASK_SIZE - sizeof(unsigned int) ||
	    ((unsigned long)ptr & 3))
		return -EFAULT;

	pagefault_disable();
	rc = __get_user_inatomic(*ret, ptr);
	pagefault_enable();

	return rc;
}

static inline void perf_callchain_user_64(struct perf_callchain_entry *entry,
					  struct pt_regs *regs)
{
}

static inline int current_is_64bit(void)
{
	return 0;
}

static inline int valid_user_sp(unsigned long sp, int is_64)
{
	if (!sp || (sp & 7) || sp > TASK_SIZE - 32)
		return 0;
	return 1;
}

#define __SIGNAL_FRAMESIZE32	__SIGNAL_FRAMESIZE
#define sigcontext32		sigcontext
#define mcontext32		mcontext
#define ucontext32		ucontext
#define compat_siginfo_t	struct siginfo

#endif 

struct signal_frame_32 {
	char			dummy[__SIGNAL_FRAMESIZE32];
	struct sigcontext32	sctx;
	struct mcontext32	mctx;
	int			abigap[56];
};

struct rt_signal_frame_32 {
	char			dummy[__SIGNAL_FRAMESIZE32 + 16];
	compat_siginfo_t	info;
	struct ucontext32	uc;
	int			abigap[56];
};

static int is_sigreturn_32_address(unsigned int nip, unsigned int fp)
{
	if (nip == fp + offsetof(struct signal_frame_32, mctx.mc_pad))
		return 1;
	if (vdso32_sigtramp && current->mm->context.vdso_base &&
	    nip == current->mm->context.vdso_base + vdso32_sigtramp)
		return 1;
	return 0;
}

static int is_rt_sigreturn_32_address(unsigned int nip, unsigned int fp)
{
	if (nip == fp + offsetof(struct rt_signal_frame_32,
				 uc.uc_mcontext.mc_pad))
		return 1;
	if (vdso32_rt_sigtramp && current->mm->context.vdso_base &&
	    nip == current->mm->context.vdso_base + vdso32_rt_sigtramp)
		return 1;
	return 0;
}

static int sane_signal_32_frame(unsigned int sp)
{
	struct signal_frame_32 __user *sf;
	unsigned int regs;

	sf = (struct signal_frame_32 __user *) (unsigned long) sp;
	if (read_user_stack_32((unsigned int __user *) &sf->sctx.regs, &regs))
		return 0;
	return regs == (unsigned long) &sf->mctx;
}

static int sane_rt_signal_32_frame(unsigned int sp)
{
	struct rt_signal_frame_32 __user *sf;
	unsigned int regs;

	sf = (struct rt_signal_frame_32 __user *) (unsigned long) sp;
	if (read_user_stack_32((unsigned int __user *) &sf->uc.uc_regs, &regs))
		return 0;
	return regs == (unsigned long) &sf->uc.uc_mcontext;
}

static unsigned int __user *signal_frame_32_regs(unsigned int sp,
				unsigned int next_sp, unsigned int next_ip)
{
	struct mcontext32 __user *mctx = NULL;
	struct signal_frame_32 __user *sf;
	struct rt_signal_frame_32 __user *rt_sf;

	if (next_sp - sp >= sizeof(struct signal_frame_32) &&
	    is_sigreturn_32_address(next_ip, sp) &&
	    sane_signal_32_frame(sp)) {
		sf = (struct signal_frame_32 __user *) (unsigned long) sp;
		mctx = &sf->mctx;
	}

	if (!mctx && next_sp - sp >= sizeof(struct rt_signal_frame_32) &&
	    is_rt_sigreturn_32_address(next_ip, sp) &&
	    sane_rt_signal_32_frame(sp)) {
		rt_sf = (struct rt_signal_frame_32 __user *) (unsigned long) sp;
		mctx = &rt_sf->uc.uc_mcontext;
	}

	if (!mctx)
		return NULL;
	return mctx->mc_gregs;
}

static void perf_callchain_user_32(struct perf_callchain_entry *entry,
				   struct pt_regs *regs)
{
	unsigned int sp, next_sp;
	unsigned int next_ip;
	unsigned int lr;
	long level = 0;
	unsigned int __user *fp, *uregs;

	next_ip = regs->nip;
	lr = regs->link;
	sp = regs->gpr[1];
	perf_callchain_store(entry, next_ip);

	while (entry->nr < PERF_MAX_STACK_DEPTH) {
		fp = (unsigned int __user *) (unsigned long) sp;
		if (!valid_user_sp(sp, 0) || read_user_stack_32(fp, &next_sp))
			return;
		if (level > 0 && read_user_stack_32(&fp[1], &next_ip))
			return;

		uregs = signal_frame_32_regs(sp, next_sp, next_ip);
		if (!uregs && level <= 1)
			uregs = signal_frame_32_regs(sp, next_sp, lr);
		if (uregs) {
			if (read_user_stack_32(&uregs[PT_NIP], &next_ip) ||
			    read_user_stack_32(&uregs[PT_LNK], &lr) ||
			    read_user_stack_32(&uregs[PT_R1], &sp))
				return;
			level = 0;
			perf_callchain_store(entry, PERF_CONTEXT_USER);
			perf_callchain_store(entry, next_ip);
			continue;
		}

		if (level == 0)
			next_ip = lr;
		perf_callchain_store(entry, next_ip);
		++level;
		sp = next_sp;
	}
}

void
perf_callchain_user(struct perf_callchain_entry *entry, struct pt_regs *regs)
{
	if (current_is_64bit())
		perf_callchain_user_64(entry, regs);
	else
		perf_callchain_user_32(entry, regs);
}
