/*
 *  Port on Texas Instruments TMS320C6x architecture
 *
 *  Copyright (C) 2004, 2006, 2009, 2010, 2011 Texas Instruments Incorporated
 *  Author: Aurelien Jacquiot (aurelien.jacquiot@jaluna.com)
 *
 *  Updated for 2.6.34: Mark Salter <msalter@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/tracehook.h>

#include <asm/ucontext.h>
#include <asm/cacheflush.h>


#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))


#define RETCODE_SIZE (9 << 2)	

struct rt_sigframe {
	struct siginfo __user *pinfo;
	void __user *puc;
	struct siginfo info;
	struct ucontext uc;
	unsigned long retcode[RETCODE_SIZE >> 2];
};

static int restore_sigcontext(struct pt_regs *regs,
			      struct sigcontext __user *sc)
{
	int err = 0;

	
#define COPY(x)  (err |= __get_user(regs->x, &sc->sc_##x))

	COPY(sp); COPY(a4); COPY(b4); COPY(a6); COPY(b6); COPY(a8); COPY(b8);
	COPY(a0); COPY(a1); COPY(a2); COPY(a3); COPY(a5); COPY(a7); COPY(a9);
	COPY(b0); COPY(b1); COPY(b2); COPY(b3); COPY(b5); COPY(b7); COPY(b9);

	COPY(a16); COPY(a17); COPY(a18); COPY(a19);
	COPY(a20); COPY(a21); COPY(a22); COPY(a23);
	COPY(a24); COPY(a25); COPY(a26); COPY(a27);
	COPY(a28); COPY(a29); COPY(a30); COPY(a31);
	COPY(b16); COPY(b17); COPY(b18); COPY(b19);
	COPY(b20); COPY(b21); COPY(b22); COPY(b23);
	COPY(b24); COPY(b25); COPY(b26); COPY(b27);
	COPY(b28); COPY(b29); COPY(b30); COPY(b31);

	COPY(csr); COPY(pc);

#undef COPY

	return err;
}

asmlinkage int do_rt_sigreturn(struct pt_regs *regs)
{
	struct rt_sigframe __user *frame;
	sigset_t set;

	if (regs->sp & 7)
		goto badframe;

	frame = (struct rt_sigframe __user *) ((unsigned long) regs->sp + 8);

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	set_current_blocked(&set);

	if (restore_sigcontext(regs, &frame->uc.uc_mcontext))
		goto badframe;

	return regs->a4;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

static int setup_sigcontext(struct sigcontext __user *sc, struct pt_regs *regs,
			    unsigned long mask)
{
	int err = 0;

	err |= __put_user(mask, &sc->sc_mask);

	
#define COPY(x) (err |= __put_user(regs->x, &sc->sc_##x))

	COPY(sp); COPY(a4); COPY(b4); COPY(a6); COPY(b6); COPY(a8); COPY(b8);
	COPY(a0); COPY(a1); COPY(a2); COPY(a3); COPY(a5); COPY(a7); COPY(a9);
	COPY(b0); COPY(b1); COPY(b2); COPY(b3); COPY(b5); COPY(b7); COPY(b9);

	COPY(a16); COPY(a17); COPY(a18); COPY(a19);
	COPY(a20); COPY(a21); COPY(a22); COPY(a23);
	COPY(a24); COPY(a25); COPY(a26); COPY(a27);
	COPY(a28); COPY(a29); COPY(a30); COPY(a31);
	COPY(b16); COPY(b17); COPY(b18); COPY(b19);
	COPY(b20); COPY(b21); COPY(b22); COPY(b23);
	COPY(b24); COPY(b25); COPY(b26); COPY(b27);
	COPY(b28); COPY(b29); COPY(b30); COPY(b31);

	COPY(csr); COPY(pc);

#undef COPY

	return err;
}

static inline void __user *get_sigframe(struct k_sigaction *ka,
					struct pt_regs *regs,
					unsigned long framesize)
{
	unsigned long sp = regs->sp;

	if ((ka->sa.sa_flags & SA_ONSTACK) && sas_ss_flags(sp) == 0)
		sp = current->sas_ss_sp + current->sas_ss_size;

	return (void __user *)((sp - framesize) & ~7);
}

static int setup_rt_frame(int signr, struct k_sigaction *ka, siginfo_t *info,
			   sigset_t *set, struct pt_regs *regs)
{
	struct rt_sigframe __user *frame;
	unsigned long __user *retcode;
	int err = 0;

	frame = get_sigframe(ka, regs, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto segv_and_exit;

	err |= __put_user(&frame->info, &frame->pinfo);
	err |= __put_user(&frame->uc, &frame->puc);
	err |= copy_siginfo_to_user(&frame->info, info);

	
	err |= __clear_user(&frame->uc, offsetof(struct ucontext, uc_mcontext));

	err |= setup_sigcontext(&frame->uc.uc_mcontext,	regs, set->sig[0]);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));

	
	retcode = (unsigned long __user *) &frame->retcode;

	
#define COPY(x) (err |= __put_user(x, retcode++))

	COPY(0x0000002AUL | (__NR_rt_sigreturn << 7));
				
	COPY(0x10000000UL);	
	COPY(0x00006000UL);	
	COPY(0x00006000UL);	
	COPY(0x00006000UL);	
	COPY(0x00006000UL);	
	COPY(0x00006000UL);	
	COPY(0x00006000UL);	
	COPY(0x00006000UL);	

#undef COPY

	if (err)
		goto segv_and_exit;

	flush_icache_range((unsigned long) &frame->retcode,
			   (unsigned long) &frame->retcode + RETCODE_SIZE);

	retcode = (unsigned long __user *) &frame->retcode;

	
	regs->sp = (unsigned long) frame - 8;
	regs->b3 = (unsigned long) retcode;
	regs->pc = (unsigned long) ka->sa.sa_handler;

	
	regs->a4 = signr;

	regs->b4 = (unsigned long)&frame->info;
	regs->a6 = (unsigned long)&frame->uc;

	return 0;

segv_and_exit:
	force_sigsegv(signr, current);
	return -EFAULT;
}

static inline void
handle_restart(struct pt_regs *regs, struct k_sigaction *ka, int has_handler)
{
	switch (regs->a4) {
	case -ERESTARTNOHAND:
		if (!has_handler)
			goto do_restart;
		regs->a4 = -EINTR;
		break;

	case -ERESTARTSYS:
		if (has_handler && !(ka->sa.sa_flags & SA_RESTART)) {
			regs->a4 = -EINTR;
			break;
		}
	
	case -ERESTARTNOINTR:
do_restart:
		regs->a4 = regs->orig_a4;
		regs->pc -= 4;
		break;
	}
}

static int handle_signal(int sig,
			 siginfo_t *info, struct k_sigaction *ka,
			 sigset_t *oldset, struct pt_regs *regs,
			 int syscall)
{
	int ret;

	
	if (syscall) {
		
		switch (regs->a4) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
			regs->a4 = -EINTR;
			break;

		case -ERESTARTSYS:
			if (!(ka->sa.sa_flags & SA_RESTART)) {
				regs->a4 = -EINTR;
				break;
			}

			
		case -ERESTARTNOINTR:
			regs->a4 = regs->orig_a4;
			regs->pc -= 4;
		}
	}

	
	ret = setup_rt_frame(sig, ka, info, oldset, regs);
	if (ret == 0)
		block_sigmask(ka, sig);

	return ret;
}

static void do_signal(struct pt_regs *regs, int syscall)
{
	struct k_sigaction ka;
	siginfo_t info;
	sigset_t *oldset;
	int signr;

	if (!user_mode(regs))
		return;

	if (test_thread_flag(TIF_RESTORE_SIGMASK))
		oldset = &current->saved_sigmask;
	else
		oldset = &current->blocked;

	signr = get_signal_to_deliver(&info, &ka, regs, NULL);
	if (signr > 0) {
		if (handle_signal(signr, &info, &ka, oldset,
				  regs, syscall) == 0) {
			if (test_thread_flag(TIF_RESTORE_SIGMASK))
				clear_thread_flag(TIF_RESTORE_SIGMASK);

			tracehook_signal_handler(signr, &info, &ka, regs, 0);
		}

		return;
	}

	
	if (syscall) {
		
		switch (regs->a4) {
		case -ERESTARTNOHAND:
		case -ERESTARTSYS:
		case -ERESTARTNOINTR:
			regs->a4 = regs->orig_a4;
			regs->pc -= 4;
			break;

		case -ERESTART_RESTARTBLOCK:
			regs->a4 = regs->orig_a4;
			regs->b0 = __NR_restart_syscall;
			regs->pc -= 4;
			break;
		}
	}

	if (test_thread_flag(TIF_RESTORE_SIGMASK)) {
		clear_thread_flag(TIF_RESTORE_SIGMASK);
		sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
	}
}

asmlinkage void do_notify_resume(struct pt_regs *regs, u32 thread_info_flags,
				 int syscall)
{
	
	if (thread_info_flags & ((1 << TIF_SIGPENDING) |
				 (1 << TIF_RESTORE_SIGMASK)))
		do_signal(regs, syscall);

	if (thread_info_flags & (1 << TIF_NOTIFY_RESUME)) {
		clear_thread_flag(TIF_NOTIFY_RESUME);
		tracehook_notify_resume(regs);
		if (current->replacement_session_keyring)
			key_replace_session_keyring();
	}
}
