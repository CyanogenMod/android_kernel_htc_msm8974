/*
 * OpenRISC signal.c
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * Modifications for the OpenRISC architecture:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/stddef.h>
#include <linux/tracehook.h>

#include <asm/processor.h>
#include <asm/ucontext.h>
#include <asm/uaccess.h>

#define DEBUG_SIG 0

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

asmlinkage long
_sys_sigaltstack(const stack_t *uss, stack_t *uoss, struct pt_regs *regs)
{
	return do_sigaltstack(uss, uoss, regs->sp);
}

struct rt_sigframe {
	struct siginfo *pinfo;
	void *puc;
	struct siginfo info;
	struct ucontext uc;
	unsigned char retcode[16];	
};

static int restore_sigcontext(struct pt_regs *regs, struct sigcontext *sc)
{
	unsigned int err = 0;

	
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

	if (__copy_from_user(regs, sc->regs.gpr, 32 * sizeof(unsigned long)))
		goto badframe;
	if (__copy_from_user(&regs->pc, &sc->regs.pc, sizeof(unsigned long)))
		goto badframe;
	if (__copy_from_user(&regs->sr, &sc->regs.sr, sizeof(unsigned long)))
		goto badframe;

	
	regs->sr &= ~SPR_SR_SM;


	return err;

badframe:
	return 1;
}

asmlinkage long _sys_rt_sigreturn(struct pt_regs *regs)
{
	struct rt_sigframe *frame = (struct rt_sigframe __user *)regs->sp;
	sigset_t set;
	stack_t st;

	if (((long)frame) & 3)
		goto badframe;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	set_current_blocked(&set);

	if (restore_sigcontext(regs, &frame->uc.uc_mcontext))
		goto badframe;

	if (__copy_from_user(&st, &frame->uc.uc_stack, sizeof(st)))
		goto badframe;
	do_sigaltstack(&st, NULL, regs->sp);

	return regs->gpr[11];

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}


static int setup_sigcontext(struct sigcontext *sc, struct pt_regs *regs,
			    unsigned long mask)
{
	int err = 0;

	

	err |= __copy_to_user(sc->regs.gpr, regs, 32 * sizeof(unsigned long));
	err |= __copy_to_user(&sc->regs.pc, &regs->pc, sizeof(unsigned long));
	err |= __copy_to_user(&sc->regs.sr, &regs->sr, sizeof(unsigned long));

	

	err |= __put_user(mask, &sc->oldmask);

	return err;
}

static inline unsigned long align_sigframe(unsigned long sp)
{
	return sp & ~3UL;
}


static inline void __user *get_sigframe(struct k_sigaction *ka,
					struct pt_regs *regs, size_t frame_size)
{
	unsigned long sp = regs->sp;
	int onsigstack = on_sig_stack(sp);

	
	sp -= STACK_FRAME_OVERHEAD;

	
	if ((ka->sa.sa_flags & SA_ONSTACK) && !onsigstack) {
		if (current->sas_ss_size)
			sp = current->sas_ss_sp + current->sas_ss_size;
	}

	sp = align_sigframe(sp - frame_size);

	if (onsigstack && !likely(on_sig_stack(sp)))
		return (void __user *)-1L;

	return (void __user *)sp;
}

static int setup_rt_frame(int sig, struct k_sigaction *ka, siginfo_t *info,
			  sigset_t *set, struct pt_regs *regs)
{
	struct rt_sigframe *frame;
	unsigned long return_ip;
	int err = 0;

	frame = get_sigframe(ka, regs, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	err |= __put_user(&frame->info, &frame->pinfo);
	err |= __put_user(&frame->uc, &frame->puc);

	if (ka->sa.sa_flags & SA_SIGINFO)
		err |= copy_siginfo_to_user(&frame->info, info);
	if (err)
		goto give_sigsegv;

	
	err |= __clear_user(&frame->uc, offsetof(struct ucontext, uc_mcontext));
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(NULL, &frame->uc.uc_link);
	err |= __put_user((void *)current->sas_ss_sp,
			  &frame->uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(regs->sp), &frame->uc.uc_stack.ss_flags);
	err |= __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size);
	err |= setup_sigcontext(&frame->uc.uc_mcontext, regs, set->sig[0]);

	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));

	if (err)
		goto give_sigsegv;

	
	return_ip = (unsigned long)&frame->retcode;
	
	err |= __put_user(0xa960, (short *)(frame->retcode + 0));
	err |= __put_user(__NR_rt_sigreturn, (short *)(frame->retcode + 2));
	err |= __put_user(0x20000001, (unsigned long *)(frame->retcode + 4));
	err |= __put_user(0x15000000, (unsigned long *)(frame->retcode + 8));

	if (err)
		goto give_sigsegv;

	

	
	regs->pc = (unsigned long)ka->sa.sa_handler; 
	regs->gpr[9] = (unsigned long)return_ip;     
	regs->gpr[3] = (unsigned long)sig;           
	regs->gpr[4] = (unsigned long)&frame->info;  
	regs->gpr[5] = (unsigned long)&frame->uc;    

	
	regs->sp = (unsigned long)frame;

	return 0;

give_sigsegv:
	force_sigsegv(sig, current);
	return -EFAULT;
}

static inline int
handle_signal(unsigned long sig,
	      siginfo_t *info, struct k_sigaction *ka,
	      sigset_t *oldset, struct pt_regs *regs)
{
	int ret;

	ret = setup_rt_frame(sig, ka, info, oldset, regs);
	if (ret)
		return ret;

	block_sigmask(ka, sig);

	return 0;
}


void do_signal(struct pt_regs *regs)
{
	siginfo_t info;
	int signr;
	struct k_sigaction ka;

	if (!user_mode(regs))
		return;

	signr = get_signal_to_deliver(&info, &ka, regs, NULL);

	if (regs->orig_gpr11) {
		int restart = 0;

		switch (regs->gpr[11]) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
			
			restart = (signr <= 0);
			break;
		case -ERESTARTSYS:
			restart = (signr <= 0 || (ka.sa.sa_flags & SA_RESTART));
			break;
		case -ERESTARTNOINTR:
			
			restart = 1;
			break;
		}

		if (restart) {
			if (regs->gpr[11] == -ERESTART_RESTARTBLOCK)
				regs->gpr[11] = __NR_restart_syscall;
			else
				regs->gpr[11] = regs->orig_gpr11;
			regs->pc -= 4;
		} else {
			regs->gpr[11] = -EINTR;
		}
	}

	if (signr <= 0) {
		if (test_thread_flag(TIF_RESTORE_SIGMASK)) {
			clear_thread_flag(TIF_RESTORE_SIGMASK);
			sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
		}

	} else {		
		sigset_t *oldset;

		if (current_thread_info()->flags & _TIF_RESTORE_SIGMASK)
			oldset = &current->saved_sigmask;
		else
			oldset = &current->blocked;

		
		if (!handle_signal(signr, &info, &ka, oldset, regs)) {
			clear_thread_flag(TIF_RESTORE_SIGMASK);
		}

		tracehook_signal_handler(signr, &info, &ka, regs,
					 test_thread_flag(TIF_SINGLESTEP));
	}

	return;
}

asmlinkage void do_notify_resume(struct pt_regs *regs)
{
	if (current_thread_info()->flags & _TIF_SIGPENDING)
		do_signal(regs);

	if (current_thread_info()->flags & _TIF_NOTIFY_RESUME) {
		clear_thread_flag(TIF_NOTIFY_RESUME);
		tracehook_notify_resume(regs);
		if (current->replacement_session_keyring)
			key_replace_session_keyring();
	}
}
