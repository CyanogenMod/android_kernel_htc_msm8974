/*
 * Signal support for Hexagon processor
 *
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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

#include <linux/linkage.h>
#include <linux/syscalls.h>
#include <linux/freezer.h>
#include <linux/tracehook.h>
#include <asm/registers.h>
#include <asm/thread_info.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <asm/ucontext.h>
#include <asm/cacheflush.h>
#include <asm/signal.h>
#include <asm/vdso.h>

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

struct rt_sigframe {
	unsigned long tramp[2];
	struct siginfo info;
	struct ucontext uc;
};

static void __user *get_sigframe(struct k_sigaction *ka, struct pt_regs *regs,
			  size_t frame_size)
{
	unsigned long sp = regs->r29;

	
	if ((ka->sa.sa_flags & SA_ONSTACK) && (sas_ss_flags(sp) == 0))
		sp = current->sas_ss_sp + current->sas_ss_size;

	return (void __user *)((sp - frame_size) & ~(sizeof(long long) - 1));
}

static int setup_sigcontext(struct pt_regs *regs, struct sigcontext __user *sc)
{
	unsigned long tmp;
	int err = 0;

	err |= copy_to_user(&sc->sc_regs.r0, &regs->r00,
			    32*sizeof(unsigned long));

	err |= __put_user(regs->sa0, &sc->sc_regs.sa0);
	err |= __put_user(regs->lc0, &sc->sc_regs.lc0);
	err |= __put_user(regs->sa1, &sc->sc_regs.sa1);
	err |= __put_user(regs->lc1, &sc->sc_regs.lc1);
	err |= __put_user(regs->m0, &sc->sc_regs.m0);
	err |= __put_user(regs->m1, &sc->sc_regs.m1);
	err |= __put_user(regs->usr, &sc->sc_regs.usr);
	err |= __put_user(regs->preds, &sc->sc_regs.p3_0);
	err |= __put_user(regs->gp, &sc->sc_regs.gp);
	err |= __put_user(regs->ugp, &sc->sc_regs.ugp);

	tmp = pt_elr(regs); err |= __put_user(tmp, &sc->sc_regs.pc);
	tmp = pt_cause(regs); err |= __put_user(tmp, &sc->sc_regs.cause);
	tmp = pt_badva(regs); err |= __put_user(tmp, &sc->sc_regs.badva);

	return err;
}

static int restore_sigcontext(struct pt_regs *regs,
			      struct sigcontext __user *sc)
{
	unsigned long tmp;
	int err = 0;

	err |= copy_from_user(&regs->r00, &sc->sc_regs.r0,
			      32 * sizeof(unsigned long));

	err |= __get_user(regs->sa0, &sc->sc_regs.sa0);
	err |= __get_user(regs->lc0, &sc->sc_regs.lc0);
	err |= __get_user(regs->sa1, &sc->sc_regs.sa1);
	err |= __get_user(regs->lc1, &sc->sc_regs.lc1);
	err |= __get_user(regs->m0, &sc->sc_regs.m0);
	err |= __get_user(regs->m1, &sc->sc_regs.m1);
	err |= __get_user(regs->usr, &sc->sc_regs.usr);
	err |= __get_user(regs->preds, &sc->sc_regs.p3_0);
	err |= __get_user(regs->gp, &sc->sc_regs.gp);
	err |= __get_user(regs->ugp, &sc->sc_regs.ugp);

	err |= __get_user(tmp, &sc->sc_regs.pc); pt_set_elr(regs, tmp);

	return err;
}

static int setup_rt_frame(int signr, struct k_sigaction *ka, siginfo_t *info,
			  sigset_t *set,  struct pt_regs *regs)
{
	int err = 0;
	struct rt_sigframe __user *frame;
	struct hexagon_vdso *vdso = current->mm->context.vdso;

	frame = get_sigframe(ka, regs, sizeof(struct rt_sigframe));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(struct rt_sigframe)))
		goto	sigsegv;

	if (copy_siginfo_to_user(&frame->info, info))
		goto	sigsegv;

	err |= __put_user(0x7800d166, &frame->tramp[0]);
	err |= __put_user(0x5400c004, &frame->tramp[1]);
	err |= setup_sigcontext(regs, &frame->uc.uc_mcontext);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));
	if (err)
		goto sigsegv;

	
	regs->r0100 = ((unsigned long long)((unsigned long)&frame->info) << 32)
		| (unsigned long long)signr;
	regs->r02 = (unsigned long) &frame->uc;
	regs->r31 = (unsigned long) vdso->rt_signal_trampoline;
	pt_psp(regs) = (unsigned long) frame;
	pt_set_elr(regs, (unsigned long)ka->sa.sa_handler);

	return 0;

sigsegv:
	force_sigsegv(signr, current);
	return -EFAULT;
}

static int handle_signal(int sig, siginfo_t *info, struct k_sigaction *ka,
			 sigset_t *oldset, struct pt_regs *regs)
{
	int rc;


	if (regs->syscall_nr >= 0) {
		switch (regs->r00) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
			regs->r00 = -EINTR;
			break;
		case -ERESTARTSYS:
			if (!(ka->sa.sa_flags & SA_RESTART)) {
				regs->r00 = -EINTR;
				break;
			}
			
		case -ERESTARTNOINTR:
			regs->r06 = regs->syscall_nr;
			pt_set_elr(regs, pt_elr(regs) - 4);
			regs->r00 = regs->restart_r0;
			break;
		default:
			break;
		}
	}

	rc = setup_rt_frame(sig, ka, info, oldset, regs);

	
	if (rc)
		return rc;

	block_sigmask(ka, sig);

	return 0;
}

static void do_signal(struct pt_regs *regs)
{
	struct k_sigaction sigact;
	siginfo_t info;
	int signo;

	if (!user_mode(regs))
		return;

	if (try_to_freeze())
		goto no_signal;

	signo = get_signal_to_deliver(&info, &sigact, regs, NULL);

	if (signo > 0) {
		sigset_t *oldset;

		if (test_thread_flag(TIF_RESTORE_SIGMASK))
			oldset = &current->saved_sigmask;
		else
			oldset = &current->blocked;

		if (handle_signal(signo, &info, &sigact, oldset, regs) == 0) {
			clear_thread_flag(TIF_RESTORE_SIGMASK);

			tracehook_signal_handler(signo, &info, &sigact, regs,
				test_thread_flag(TIF_SINGLESTEP));
		}
		return;
	}

no_signal:
	if (regs->syscall_nr >= 0) {
		switch (regs->r00) {
		case -ERESTARTNOHAND:
		case -ERESTARTSYS:
		case -ERESTARTNOINTR:
			regs->r06 = regs->syscall_nr;
			break;
		case -ERESTART_RESTARTBLOCK:
			regs->r06 = __NR_restart_syscall;
			break;
		default:
			goto no_restart;
		}
		pt_set_elr(regs, pt_elr(regs) - 4);
		regs->r00 = regs->restart_r0;
	}

no_restart:
	
	if (test_thread_flag(TIF_RESTORE_SIGMASK)) {
		clear_thread_flag(TIF_RESTORE_SIGMASK);
		sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
	}
}

void do_notify_resume(struct pt_regs *regs, unsigned long thread_info_flags)
{
	if (thread_info_flags & _TIF_SIGPENDING)
		do_signal(regs);

	if (thread_info_flags & _TIF_NOTIFY_RESUME) {
		clear_thread_flag(TIF_NOTIFY_RESUME);
		if (current->replacement_session_keyring)
			key_replace_session_keyring();
	}
}

asmlinkage int sys_sigaltstack(const stack_t __user *uss, stack_t __user *uoss)
{
	struct pt_regs *regs = current_thread_info()->regs;

	return do_sigaltstack(uss, uoss, regs->r29);
}

asmlinkage int sys_rt_sigreturn(void)
{
	struct pt_regs *regs = current_thread_info()->regs;
	struct rt_sigframe __user *frame;
	sigset_t blocked;

	frame = (struct rt_sigframe __user *)pt_psp(regs);
	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&blocked, &frame->uc.uc_sigmask, sizeof(blocked)))
		goto badframe;

	sigdelsetmask(&blocked, ~_BLOCKABLE);
	set_current_blocked(&blocked);

	if (restore_sigcontext(regs, &frame->uc.uc_mcontext))
		goto badframe;

	
	pt_psp(regs) = regs->r29;

	regs->syscall_nr = __NR_rt_sigreturn;

	if (do_sigaltstack(&frame->uc.uc_stack, NULL, pt_psp(regs)) == -EFAULT)
		goto badframe;

	return 0;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}
