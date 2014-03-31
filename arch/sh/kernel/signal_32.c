/*
 *  linux/arch/sh/kernel/signal.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  1997-11-28  Modified for POSIX.1b signals by Richard Henderson
 *
 *  SuperH version:  Copyright (C) 1999, 2000  Niibe Yutaka & Kaz Kojima
 *
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
#include <linux/tty.h>
#include <linux/elf.h>
#include <linux/personality.h>
#include <linux/binfmts.h>
#include <linux/freezer.h>
#include <linux/io.h>
#include <linux/tracehook.h>
#include <asm/ucontext.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/cacheflush.h>
#include <asm/syscalls.h>
#include <asm/fpu.h>

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

struct fdpic_func_descriptor {
	unsigned long	text;
	unsigned long	GOT;
};

#define UNWINDGUARD 64

asmlinkage int
sys_sigsuspend(old_sigset_t mask,
	       unsigned long r5, unsigned long r6, unsigned long r7,
	       struct pt_regs __regs)
{
	sigset_t blocked;

	current->saved_sigmask = current->blocked;

	mask &= _BLOCKABLE;
	siginitset(&blocked, mask);
	set_current_blocked(&blocked);

	current->state = TASK_INTERRUPTIBLE;
	schedule();
	set_restore_sigmask();

	return -ERESTARTNOHAND;
}

asmlinkage int
sys_sigaction(int sig, const struct old_sigaction __user *act,
	      struct old_sigaction __user *oact)
{
	struct k_sigaction new_ka, old_ka;
	int ret;

	if (act) {
		old_sigset_t mask;
		if (!access_ok(VERIFY_READ, act, sizeof(*act)) ||
		    __get_user(new_ka.sa.sa_handler, &act->sa_handler) ||
		    __get_user(new_ka.sa.sa_restorer, &act->sa_restorer))
			return -EFAULT;
		__get_user(new_ka.sa.sa_flags, &act->sa_flags);
		__get_user(mask, &act->sa_mask);
		siginitset(&new_ka.sa.sa_mask, mask);
	}

	ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

	if (!ret && oact) {
		if (!access_ok(VERIFY_WRITE, oact, sizeof(*oact)) ||
		    __put_user(old_ka.sa.sa_handler, &oact->sa_handler) ||
		    __put_user(old_ka.sa.sa_restorer, &oact->sa_restorer))
			return -EFAULT;
		__put_user(old_ka.sa.sa_flags, &oact->sa_flags);
		__put_user(old_ka.sa.sa_mask.sig[0], &oact->sa_mask);
	}

	return ret;
}

asmlinkage int
sys_sigaltstack(const stack_t __user *uss, stack_t __user *uoss,
		unsigned long r6, unsigned long r7,
		struct pt_regs __regs)
{
	struct pt_regs *regs = RELOC_HIDE(&__regs, 0);

	return do_sigaltstack(uss, uoss, regs->regs[15]);
}



#define MOVW(n)	 (0x9300|((n)-2))	
#if defined(CONFIG_CPU_SH2)
#define TRAP_NOARG 0xc320		
#else
#define TRAP_NOARG 0xc310		
#endif
#define OR_R0_R0 0x200b			

struct sigframe
{
	struct sigcontext sc;
	unsigned long extramask[_NSIG_WORDS-1];
	u16 retcode[8];
};

struct rt_sigframe
{
	struct siginfo info;
	struct ucontext uc;
	u16 retcode[8];
};

#ifdef CONFIG_SH_FPU
static inline int restore_sigcontext_fpu(struct sigcontext __user *sc)
{
	struct task_struct *tsk = current;

	if (!(boot_cpu_data.flags & CPU_HAS_FPU))
		return 0;

	set_used_math();
	return __copy_from_user(&tsk->thread.xstate->hardfpu, &sc->sc_fpregs[0],
				sizeof(long)*(16*2+2));
}

static inline int save_sigcontext_fpu(struct sigcontext __user *sc,
				      struct pt_regs *regs)
{
	struct task_struct *tsk = current;

	if (!(boot_cpu_data.flags & CPU_HAS_FPU))
		return 0;

	if (!used_math()) {
		__put_user(0, &sc->sc_ownedfp);
		return 0;
	}

	__put_user(1, &sc->sc_ownedfp);

	clear_used_math();

	unlazy_fpu(tsk, regs);
	return __copy_to_user(&sc->sc_fpregs[0], &tsk->thread.xstate->hardfpu,
			      sizeof(long)*(16*2+2));
}
#endif 

static int
restore_sigcontext(struct pt_regs *regs, struct sigcontext __user *sc, int *r0_p)
{
	unsigned int err = 0;

#define COPY(x)		err |= __get_user(regs->x, &sc->sc_##x)
			COPY(regs[1]);
	COPY(regs[2]);	COPY(regs[3]);
	COPY(regs[4]);	COPY(regs[5]);
	COPY(regs[6]);	COPY(regs[7]);
	COPY(regs[8]);	COPY(regs[9]);
	COPY(regs[10]);	COPY(regs[11]);
	COPY(regs[12]);	COPY(regs[13]);
	COPY(regs[14]);	COPY(regs[15]);
	COPY(gbr);	COPY(mach);
	COPY(macl);	COPY(pr);
	COPY(sr);	COPY(pc);
#undef COPY

#ifdef CONFIG_SH_FPU
	if (boot_cpu_data.flags & CPU_HAS_FPU) {
		int owned_fp;
		struct task_struct *tsk = current;

		regs->sr |= SR_FD; 
		clear_fpu(tsk, regs);
		clear_used_math();
		__get_user (owned_fp, &sc->sc_ownedfp);
		if (owned_fp)
			err |= restore_sigcontext_fpu(sc);
	}
#endif

	regs->tra = -1;		
	err |= __get_user(*r0_p, &sc->sc_regs[0]);
	return err;
}

asmlinkage int sys_sigreturn(unsigned long r4, unsigned long r5,
			     unsigned long r6, unsigned long r7,
			     struct pt_regs __regs)
{
	struct pt_regs *regs = RELOC_HIDE(&__regs, 0);
	struct sigframe __user *frame = (struct sigframe __user *)regs->regs[15];
	sigset_t set;
	int r0;

        
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;

	if (__get_user(set.sig[0], &frame->sc.oldmask)
	    || (_NSIG_WORDS > 1
		&& __copy_from_user(&set.sig[1], &frame->extramask,
				    sizeof(frame->extramask))))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	set_current_blocked(&set);

	if (restore_sigcontext(regs, &frame->sc, &r0))
		goto badframe;
	return r0;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

asmlinkage int sys_rt_sigreturn(unsigned long r4, unsigned long r5,
				unsigned long r6, unsigned long r7,
				struct pt_regs __regs)
{
	struct pt_regs *regs = RELOC_HIDE(&__regs, 0);
	struct rt_sigframe __user *frame = (struct rt_sigframe __user *)regs->regs[15];
	sigset_t set;
	int r0;

	
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;

	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	set_current_blocked(&set);

	if (restore_sigcontext(regs, &frame->uc.uc_mcontext, &r0))
		goto badframe;

	if (do_sigaltstack(&frame->uc.uc_stack, NULL,
			   regs->regs[15]) == -EFAULT)
		goto badframe;

	return r0;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}


static int
setup_sigcontext(struct sigcontext __user *sc, struct pt_regs *regs,
		 unsigned long mask)
{
	int err = 0;

#define COPY(x)		err |= __put_user(regs->x, &sc->sc_##x)
	COPY(regs[0]);	COPY(regs[1]);
	COPY(regs[2]);	COPY(regs[3]);
	COPY(regs[4]);	COPY(regs[5]);
	COPY(regs[6]);	COPY(regs[7]);
	COPY(regs[8]);	COPY(regs[9]);
	COPY(regs[10]);	COPY(regs[11]);
	COPY(regs[12]);	COPY(regs[13]);
	COPY(regs[14]);	COPY(regs[15]);
	COPY(gbr);	COPY(mach);
	COPY(macl);	COPY(pr);
	COPY(sr);	COPY(pc);
#undef COPY

#ifdef CONFIG_SH_FPU
	err |= save_sigcontext_fpu(sc, regs);
#endif

	
	err |= __put_user(mask, &sc->oldmask);

	return err;
}

static inline void __user *
get_sigframe(struct k_sigaction *ka, unsigned long sp, size_t frame_size)
{
	if (ka->sa.sa_flags & SA_ONSTACK) {
		if (sas_ss_flags(sp) == 0)
			sp = current->sas_ss_sp + current->sas_ss_size;
	}

	return (void __user *)((sp - (frame_size+UNWINDGUARD)) & -8ul);
}

extern void __kernel_sigreturn(void);
extern void __kernel_rt_sigreturn(void);

static int setup_frame(int sig, struct k_sigaction *ka,
			sigset_t *set, struct pt_regs *regs)
{
	struct sigframe __user *frame;
	int err = 0;
	int signal;

	frame = get_sigframe(ka, regs->regs[15], sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	signal = current_thread_info()->exec_domain
		&& current_thread_info()->exec_domain->signal_invmap
		&& sig < 32
		? current_thread_info()->exec_domain->signal_invmap[sig]
		: sig;

	err |= setup_sigcontext(&frame->sc, regs, set->sig[0]);

	if (_NSIG_WORDS > 1)
		err |= __copy_to_user(frame->extramask, &set->sig[1],
				      sizeof(frame->extramask));

	if (ka->sa.sa_flags & SA_RESTORER) {
		regs->pr = (unsigned long) ka->sa.sa_restorer;
#ifdef CONFIG_VSYSCALL
	} else if (likely(current->mm->context.vdso)) {
		regs->pr = VDSO_SYM(&__kernel_sigreturn);
#endif
	} else {
		
		err |= __put_user(MOVW(7), &frame->retcode[0]);
		err |= __put_user(TRAP_NOARG, &frame->retcode[1]);
		err |= __put_user(OR_R0_R0, &frame->retcode[2]);
		err |= __put_user(OR_R0_R0, &frame->retcode[3]);
		err |= __put_user(OR_R0_R0, &frame->retcode[4]);
		err |= __put_user(OR_R0_R0, &frame->retcode[5]);
		err |= __put_user(OR_R0_R0, &frame->retcode[6]);
		err |= __put_user((__NR_sigreturn), &frame->retcode[7]);
		regs->pr = (unsigned long) frame->retcode;
		flush_icache_range(regs->pr, regs->pr + sizeof(frame->retcode));
	}

	if (err)
		goto give_sigsegv;

	
	regs->regs[15] = (unsigned long) frame;
	regs->regs[4] = signal; 
	regs->regs[5] = 0;
	regs->regs[6] = (unsigned long) &frame->sc;

	if (current->personality & FDPIC_FUNCPTRS) {
		struct fdpic_func_descriptor __user *funcptr =
			(struct fdpic_func_descriptor __user *)ka->sa.sa_handler;

		__get_user(regs->pc, &funcptr->text);
		__get_user(regs->regs[12], &funcptr->GOT);
	} else
		regs->pc = (unsigned long)ka->sa.sa_handler;

	set_fs(USER_DS);

	pr_debug("SIG deliver (%s:%d): sp=%p pc=%08lx pr=%08lx\n",
		 current->comm, task_pid_nr(current), frame, regs->pc, regs->pr);

	return 0;

give_sigsegv:
	force_sigsegv(sig, current);
	return -EFAULT;
}

static int setup_rt_frame(int sig, struct k_sigaction *ka, siginfo_t *info,
			   sigset_t *set, struct pt_regs *regs)
{
	struct rt_sigframe __user *frame;
	int err = 0;
	int signal;

	frame = get_sigframe(ka, regs->regs[15], sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	signal = current_thread_info()->exec_domain
		&& current_thread_info()->exec_domain->signal_invmap
		&& sig < 32
		? current_thread_info()->exec_domain->signal_invmap[sig]
		: sig;

	err |= copy_siginfo_to_user(&frame->info, info);

	
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(NULL, &frame->uc.uc_link);
	err |= __put_user((void *)current->sas_ss_sp,
			  &frame->uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(regs->regs[15]),
			  &frame->uc.uc_stack.ss_flags);
	err |= __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size);
	err |= setup_sigcontext(&frame->uc.uc_mcontext,
			        regs, set->sig[0]);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));

	if (ka->sa.sa_flags & SA_RESTORER) {
		regs->pr = (unsigned long) ka->sa.sa_restorer;
#ifdef CONFIG_VSYSCALL
	} else if (likely(current->mm->context.vdso)) {
		regs->pr = VDSO_SYM(&__kernel_rt_sigreturn);
#endif
	} else {
		
		err |= __put_user(MOVW(7), &frame->retcode[0]);
		err |= __put_user(TRAP_NOARG, &frame->retcode[1]);
		err |= __put_user(OR_R0_R0, &frame->retcode[2]);
		err |= __put_user(OR_R0_R0, &frame->retcode[3]);
		err |= __put_user(OR_R0_R0, &frame->retcode[4]);
		err |= __put_user(OR_R0_R0, &frame->retcode[5]);
		err |= __put_user(OR_R0_R0, &frame->retcode[6]);
		err |= __put_user((__NR_rt_sigreturn), &frame->retcode[7]);
		regs->pr = (unsigned long) frame->retcode;
		flush_icache_range(regs->pr, regs->pr + sizeof(frame->retcode));
	}

	if (err)
		goto give_sigsegv;

	
	regs->regs[15] = (unsigned long) frame;
	regs->regs[4] = signal; 
	regs->regs[5] = (unsigned long) &frame->info;
	regs->regs[6] = (unsigned long) &frame->uc;

	if (current->personality & FDPIC_FUNCPTRS) {
		struct fdpic_func_descriptor __user *funcptr =
			(struct fdpic_func_descriptor __user *)ka->sa.sa_handler;

		__get_user(regs->pc, &funcptr->text);
		__get_user(regs->regs[12], &funcptr->GOT);
	} else
		regs->pc = (unsigned long)ka->sa.sa_handler;

	set_fs(USER_DS);

	pr_debug("SIG deliver (%s:%d): sp=%p pc=%08lx pr=%08lx\n",
		 current->comm, task_pid_nr(current), frame, regs->pc, regs->pr);

	return 0;

give_sigsegv:
	force_sigsegv(sig, current);
	return -EFAULT;
}

static inline void
handle_syscall_restart(unsigned long save_r0, struct pt_regs *regs,
		       struct sigaction *sa)
{
	
	if (regs->tra < 0)
		return;

	
	switch (regs->regs[0]) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
		no_system_call_restart:
			regs->regs[0] = -EINTR;
			break;

		case -ERESTARTSYS:
			if (!(sa->sa_flags & SA_RESTART))
				goto no_system_call_restart;
		
		case -ERESTARTNOINTR:
			regs->regs[0] = save_r0;
			regs->pc -= instruction_size(__raw_readw(regs->pc - 4));
			break;
	}
}

static int
handle_signal(unsigned long sig, struct k_sigaction *ka, siginfo_t *info,
	      sigset_t *oldset, struct pt_regs *regs, unsigned int save_r0)
{
	int ret;

	
	if (ka->sa.sa_flags & SA_SIGINFO)
		ret = setup_rt_frame(sig, ka, info, oldset, regs);
	else
		ret = setup_frame(sig, ka, oldset, regs);

	if (ret == 0)
		block_sigmask(ka, sig);

	return ret;
}

static void do_signal(struct pt_regs *regs, unsigned int save_r0)
{
	siginfo_t info;
	int signr;
	struct k_sigaction ka;
	sigset_t *oldset;

	if (!user_mode(regs))
		return;

	if (current_thread_info()->status & TS_RESTORE_SIGMASK)
		oldset = &current->saved_sigmask;
	else
		oldset = &current->blocked;

	signr = get_signal_to_deliver(&info, &ka, regs, NULL);
	if (signr > 0) {
		handle_syscall_restart(save_r0, regs, &ka.sa);

		
		if (handle_signal(signr, &ka, &info, oldset,
				  regs, save_r0) == 0) {
			current_thread_info()->status &= ~TS_RESTORE_SIGMASK;

			tracehook_signal_handler(signr, &info, &ka, regs,
					test_thread_flag(TIF_SINGLESTEP));
		}

		return;
	}

	
	if (regs->tra >= 0) {
		
		if (regs->regs[0] == -ERESTARTNOHAND ||
		    regs->regs[0] == -ERESTARTSYS ||
		    regs->regs[0] == -ERESTARTNOINTR) {
			regs->regs[0] = save_r0;
			regs->pc -= instruction_size(__raw_readw(regs->pc - 4));
		} else if (regs->regs[0] == -ERESTART_RESTARTBLOCK) {
			regs->pc -= instruction_size(__raw_readw(regs->pc - 4));
			regs->regs[3] = __NR_restart_syscall;
		}
	}

	if (current_thread_info()->status & TS_RESTORE_SIGMASK) {
		current_thread_info()->status &= ~TS_RESTORE_SIGMASK;
		sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
	}
}

asmlinkage void do_notify_resume(struct pt_regs *regs, unsigned int save_r0,
				 unsigned long thread_info_flags)
{
	
	if (thread_info_flags & _TIF_SIGPENDING)
		do_signal(regs, save_r0);

	if (thread_info_flags & _TIF_NOTIFY_RESUME) {
		clear_thread_flag(TIF_NOTIFY_RESUME);
		tracehook_notify_resume(regs);
		if (current->replacement_session_keyring)
			key_replace_session_keyring();
	}
}
