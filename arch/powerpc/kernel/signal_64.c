/*
 *  PowerPC version 
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Derived from "arch/i386/kernel/signal.c"
 *    Copyright (C) 1991, 1992 Linus Torvalds
 *    1997-11-28  Modified for POSIX.1b signals by Richard Henderson
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/unistd.h>
#include <linux/stddef.h>
#include <linux/elf.h>
#include <linux/ptrace.h>
#include <linux/ratelimit.h>

#include <asm/sigcontext.h>
#include <asm/ucontext.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/unistd.h>
#include <asm/cacheflush.h>
#include <asm/syscalls.h>
#include <asm/vdso.h>
#include <asm/switch_to.h>

#include "signal.h"

#define DEBUG_SIG 0

#define GP_REGS_SIZE	min(sizeof(elf_gregset_t), sizeof(struct pt_regs))
#define FP_REGS_SIZE	sizeof(elf_fpregset_t)

#define TRAMP_TRACEBACK	3
#define TRAMP_SIZE	6


struct rt_sigframe {
	
	struct ucontext uc;
	unsigned long _unused[2];
	unsigned int tramp[TRAMP_SIZE];
	struct siginfo __user *pinfo;
	void __user *puc;
	struct siginfo info;
	
	char abigap[288];
} __attribute__ ((aligned (16)));

static const char fmt32[] = KERN_INFO \
	"%s[%d]: bad frame in %s: %08lx nip %08lx lr %08lx\n";
static const char fmt64[] = KERN_INFO \
	"%s[%d]: bad frame in %s: %016lx nip %016lx lr %016lx\n";


static long setup_sigcontext(struct sigcontext __user *sc, struct pt_regs *regs,
		 int signr, sigset_t *set, unsigned long handler,
		 int ctx_has_vsx_region)
{
#ifdef CONFIG_ALTIVEC
	elf_vrreg_t __user *v_regs = (elf_vrreg_t __user *)(((unsigned long)sc->vmx_reserve + 15) & ~0xful);
#endif
	unsigned long msr = regs->msr;
	long err = 0;

	flush_fp_to_thread(current);

#ifdef CONFIG_ALTIVEC
	err |= __put_user(v_regs, &sc->v_regs);

	
	if (current->thread.used_vr) {
		flush_altivec_to_thread(current);
		
		err |= __copy_to_user(v_regs, current->thread.vr, 33 * sizeof(vector128));
		msr |= MSR_VEC;
	}
	err |= __put_user(current->thread.vrsave, (u32 __user *)&v_regs[33]);
#else 
	err |= __put_user(0, &sc->v_regs);
#endif 
	flush_fp_to_thread(current);
	
	err |= copy_fpr_to_user(&sc->fp_regs, current);
#ifdef CONFIG_VSX
	if (current->thread.used_vsr && ctx_has_vsx_region) {
		__giveup_vsx(current);
		v_regs += ELF_NVRREG;
		err |= copy_vsx_to_user(v_regs, current);
		msr |= MSR_VSX;
	}
#endif 
	err |= __put_user(&sc->gp_regs, &sc->regs);
	WARN_ON(!FULL_REGS(regs));
	err |= __copy_to_user(&sc->gp_regs, regs, GP_REGS_SIZE);
	err |= __put_user(msr, &sc->gp_regs[PT_MSR]);
	err |= __put_user(signr, &sc->signal);
	err |= __put_user(handler, &sc->handler);
	if (set != NULL)
		err |=  __put_user(set->sig[0], &sc->oldmask);

	return err;
}


static long restore_sigcontext(struct pt_regs *regs, sigset_t *set, int sig,
			      struct sigcontext __user *sc)
{
#ifdef CONFIG_ALTIVEC
	elf_vrreg_t __user *v_regs;
#endif
	unsigned long err = 0;
	unsigned long save_r13 = 0;
	unsigned long msr;
#ifdef CONFIG_VSX
	int i;
#endif

	
	if (!sig)
		save_r13 = regs->gpr[13];

	
	err |= __copy_from_user(regs->gpr, sc->gp_regs, sizeof(regs->gpr));
	err |= __get_user(regs->nip, &sc->gp_regs[PT_NIP]);
	
	err |= __get_user(msr, &sc->gp_regs[PT_MSR]);
	if (sig)
		regs->msr = (regs->msr & ~MSR_LE) | (msr & MSR_LE);
	err |= __get_user(regs->orig_gpr3, &sc->gp_regs[PT_ORIG_R3]);
	err |= __get_user(regs->ctr, &sc->gp_regs[PT_CTR]);
	err |= __get_user(regs->link, &sc->gp_regs[PT_LNK]);
	err |= __get_user(regs->xer, &sc->gp_regs[PT_XER]);
	err |= __get_user(regs->ccr, &sc->gp_regs[PT_CCR]);
	
	regs->trap = 0;
	err |= __get_user(regs->dar, &sc->gp_regs[PT_DAR]);
	err |= __get_user(regs->dsisr, &sc->gp_regs[PT_DSISR]);
	err |= __get_user(regs->result, &sc->gp_regs[PT_RESULT]);

	if (!sig)
		regs->gpr[13] = save_r13;
	if (set != NULL)
		err |=  __get_user(set->sig[0], &sc->oldmask);

	discard_lazy_cpu_state();

	regs->msr &= ~(MSR_FP | MSR_FE0 | MSR_FE1 | MSR_VEC | MSR_VSX);

#ifdef CONFIG_ALTIVEC
	err |= __get_user(v_regs, &sc->v_regs);
	if (err)
		return err;
	if (v_regs && !access_ok(VERIFY_READ, v_regs, 34 * sizeof(vector128)))
		return -EFAULT;
	
	if (v_regs != 0 && (msr & MSR_VEC) != 0)
		err |= __copy_from_user(current->thread.vr, v_regs,
					33 * sizeof(vector128));
	else if (current->thread.used_vr)
		memset(current->thread.vr, 0, 33 * sizeof(vector128));
	
	if (v_regs != 0)
		err |= __get_user(current->thread.vrsave, (u32 __user *)&v_regs[33]);
	else
		current->thread.vrsave = 0;
#endif 
	
	err |= copy_fpr_from_user(current, &sc->fp_regs);
#ifdef CONFIG_VSX
	v_regs += ELF_NVRREG;
	if ((msr & MSR_VSX) != 0)
		err |= copy_vsx_from_user(current, v_regs);
	else
		for (i = 0; i < 32 ; i++)
			current->thread.fpr[i][TS_VSRLOWOFFSET] = 0;
#endif
	return err;
}

static long setup_trampoline(unsigned int syscall, unsigned int __user *tramp)
{
	int i;
	long err = 0;

	
	err |= __put_user(0x38210000UL | (__SIGNAL_FRAMESIZE & 0xffff), &tramp[0]);
	
	err |= __put_user(0x38000000UL | (syscall & 0xffff), &tramp[1]);
	
	err |= __put_user(0x44000002UL, &tramp[2]);

	
	for (i=TRAMP_TRACEBACK; i < TRAMP_SIZE ;i++)
		err |= __put_user(0, &tramp[i]);

	if (!err)
		flush_icache_range((unsigned long) &tramp[0],
			   (unsigned long) &tramp[TRAMP_SIZE]);

	return err;
}

#define UCONTEXTSIZEWITHOUTVSX \
		(sizeof(struct ucontext) - 32*sizeof(long))

int sys_swapcontext(struct ucontext __user *old_ctx,
		    struct ucontext __user *new_ctx,
		    long ctx_size, long r6, long r7, long r8, struct pt_regs *regs)
{
	unsigned char tmp;
	sigset_t set;
	unsigned long new_msr = 0;
	int ctx_has_vsx_region = 0;

	if (new_ctx &&
	    get_user(new_msr, &new_ctx->uc_mcontext.gp_regs[PT_MSR]))
		return -EFAULT;
	if (ctx_size < UCONTEXTSIZEWITHOUTVSX)
		return -EINVAL;
	if ((ctx_size < sizeof(struct ucontext)) &&
	    (new_msr & MSR_VSX))
		return -EINVAL;
	
	if (ctx_size >= sizeof(struct ucontext))
		ctx_has_vsx_region = 1;

	if (old_ctx != NULL) {
		if (!access_ok(VERIFY_WRITE, old_ctx, ctx_size)
		    || setup_sigcontext(&old_ctx->uc_mcontext, regs, 0, NULL, 0,
					ctx_has_vsx_region)
		    || __copy_to_user(&old_ctx->uc_sigmask,
				      &current->blocked, sizeof(sigset_t)))
			return -EFAULT;
	}
	if (new_ctx == NULL)
		return 0;
	if (!access_ok(VERIFY_READ, new_ctx, ctx_size)
	    || __get_user(tmp, (u8 __user *) new_ctx)
	    || __get_user(tmp, (u8 __user *) new_ctx + ctx_size - 1))
		return -EFAULT;


	if (__copy_from_user(&set, &new_ctx->uc_sigmask, sizeof(set)))
		do_exit(SIGSEGV);
	restore_sigmask(&set);
	if (restore_sigcontext(regs, NULL, 0, &new_ctx->uc_mcontext))
		do_exit(SIGSEGV);

	
	set_thread_flag(TIF_RESTOREALL);
	return 0;
}



int sys_rt_sigreturn(unsigned long r3, unsigned long r4, unsigned long r5,
		     unsigned long r6, unsigned long r7, unsigned long r8,
		     struct pt_regs *regs)
{
	struct ucontext __user *uc = (struct ucontext __user *)regs->gpr[1];
	sigset_t set;

	
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

	if (!access_ok(VERIFY_READ, uc, sizeof(*uc)))
		goto badframe;

	if (__copy_from_user(&set, &uc->uc_sigmask, sizeof(set)))
		goto badframe;
	restore_sigmask(&set);
	if (restore_sigcontext(regs, NULL, 1, &uc->uc_mcontext))
		goto badframe;

	do_sigaltstack(&uc->uc_stack, NULL, regs->gpr[1]);

	set_thread_flag(TIF_RESTOREALL);
	return 0;

badframe:
#if DEBUG_SIG
	printk("badframe in sys_rt_sigreturn, regs=%p uc=%p &uc->uc_mcontext=%p\n",
	       regs, uc, &uc->uc_mcontext);
#endif
	if (show_unhandled_signals)
		printk_ratelimited(regs->msr & MSR_64BIT ? fmt64 : fmt32,
				   current->comm, current->pid, "rt_sigreturn",
				   (long)uc, regs->nip, regs->link);

	force_sig(SIGSEGV, current);
	return 0;
}

int handle_rt_signal64(int signr, struct k_sigaction *ka, siginfo_t *info,
		sigset_t *set, struct pt_regs *regs)
{
	func_descr_t __user *funct_desc_ptr;
	struct rt_sigframe __user *frame;
	unsigned long newsp = 0;
	long err = 0;

	frame = get_sigframe(ka, regs, sizeof(*frame), 0);
	if (unlikely(frame == NULL))
		goto badframe;

	err |= __put_user(&frame->info, &frame->pinfo);
	err |= __put_user(&frame->uc, &frame->puc);
	err |= copy_siginfo_to_user(&frame->info, info);
	if (err)
		goto badframe;

	
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(0, &frame->uc.uc_link);
	err |= __put_user(current->sas_ss_sp, &frame->uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(regs->gpr[1]),
			  &frame->uc.uc_stack.ss_flags);
	err |= __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size);
	err |= setup_sigcontext(&frame->uc.uc_mcontext, regs, signr, NULL,
				(unsigned long)ka->sa.sa_handler, 1);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));
	if (err)
		goto badframe;

	
	current->thread.fpscr.val = 0;

	
	if (vdso64_rt_sigtramp && current->mm->context.vdso_base) {
		regs->link = current->mm->context.vdso_base + vdso64_rt_sigtramp;
	} else {
		err |= setup_trampoline(__NR_rt_sigreturn, &frame->tramp[0]);
		if (err)
			goto badframe;
		regs->link = (unsigned long) &frame->tramp[0];
	}
	funct_desc_ptr = (func_descr_t __user *) ka->sa.sa_handler;

	
	newsp = ((unsigned long)frame) - __SIGNAL_FRAMESIZE;
	err |= put_user(regs->gpr[1], (unsigned long __user *)newsp);

	
	err |= get_user(regs->nip, &funct_desc_ptr->entry);
	
	regs->msr &= ~MSR_LE;
	regs->gpr[1] = newsp;
	err |= get_user(regs->gpr[2], &funct_desc_ptr->toc);
	regs->gpr[3] = signr;
	regs->result = 0;
	if (ka->sa.sa_flags & SA_SIGINFO) {
		err |= get_user(regs->gpr[4], (unsigned long __user *)&frame->pinfo);
		err |= get_user(regs->gpr[5], (unsigned long __user *)&frame->puc);
		regs->gpr[6] = (unsigned long) frame;
	} else {
		regs->gpr[4] = (unsigned long)&frame->uc.uc_mcontext;
	}
	if (err)
		goto badframe;

	return 1;

badframe:
#if DEBUG_SIG
	printk("badframe in setup_rt_frame, regs=%p frame=%p newsp=%lx\n",
	       regs, frame, newsp);
#endif
	if (show_unhandled_signals)
		printk_ratelimited(regs->msr & MSR_64BIT ? fmt64 : fmt32,
				   current->comm, current->pid, "setup_rt_frame",
				   (long)frame, regs->nip, regs->link);

	force_sigsegv(signr, current);
	return 0;
}
