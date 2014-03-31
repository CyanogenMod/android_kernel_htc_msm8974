/*
 * Copyright (C) 2003, Axis Communications AB.
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/stddef.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>

#include <asm/io.h>
#include <asm/processor.h>
#include <asm/ucontext.h>
#include <asm/uaccess.h>
#include <arch/ptrace.h>
#include <arch/hwregs/cpu_vect.h>

extern unsigned long cris_signal_return_page;

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

#define RESTART_CRIS_SYS(regs) regs->r10 = regs->orig_r10; regs->erp -= 2;

struct signal_frame {
	struct sigcontext sc;
	unsigned long extramask[_NSIG_WORDS - 1];
	unsigned char retcode[8];	
};

struct rt_signal_frame {
	struct siginfo *pinfo;
	void *puc;
	struct siginfo info;
	struct ucontext uc;
	unsigned char retcode[8];	
};

void do_signal(int restart, struct pt_regs *regs);
void keep_debug_flags(unsigned long oldccs, unsigned long oldspc,
		      struct pt_regs *regs);
int
sys_sigsuspend(old_sigset_t mask, long r11, long r12, long r13, long mof,
	       long srp, struct pt_regs *regs)
{
	mask &= _BLOCKABLE;
	spin_lock_irq(&current->sighand->siglock);
	current->saved_sigmask = current->blocked;
	siginitset(&current->blocked, mask);
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	set_thread_flag(TIF_RESTORE_SIGMASK);
	return -ERESTARTNOHAND;
}

int
sys_sigaction(int signal, const struct old_sigaction *act,
	      struct old_sigaction *oact)
{
	int retval;
	struct k_sigaction newk;
	struct k_sigaction oldk;

	if (act) {
		old_sigset_t mask;

		if (!access_ok(VERIFY_READ, act, sizeof(*act)) ||
		    __get_user(newk.sa.sa_handler, &act->sa_handler) ||
		    __get_user(newk.sa.sa_restorer, &act->sa_restorer))
			return -EFAULT;

		__get_user(newk.sa.sa_flags, &act->sa_flags);
		__get_user(mask, &act->sa_mask);
		siginitset(&newk.sa.sa_mask, mask);
	}

	retval = do_sigaction(signal, act ? &newk : NULL, oact ? &oldk : NULL);

	if (!retval && oact) {
		if (!access_ok(VERIFY_WRITE, oact, sizeof(*oact)) ||
		    __put_user(oldk.sa.sa_handler, &oact->sa_handler) ||
		    __put_user(oldk.sa.sa_restorer, &oact->sa_restorer))
			return -EFAULT;

		__put_user(oldk.sa.sa_flags, &oact->sa_flags);
		__put_user(oldk.sa.sa_mask.sig[0], &oact->sa_mask);
	}

	return retval;
}

int
sys_sigaltstack(const stack_t __user *uss, stack_t __user *uoss)
{
	return do_sigaltstack(uss, uoss, rdusp());
}

static int
restore_sigcontext(struct pt_regs *regs, struct sigcontext __user *sc)
{
	unsigned int err = 0;
	unsigned long old_usp;

        
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

	if (__copy_from_user(regs, sc, sizeof(struct pt_regs)))
		goto badframe;

	
	regs->ccs |= (1 << (U_CCS_BITNR + CCS_SHIFT));

	
	err |= __get_user(old_usp, &sc->usp);
	wrusp(old_usp);

	return err;

badframe:
	return 1;
}

asmlinkage int
sys_sigreturn(long r10, long r11, long r12, long r13, long mof, long srp,
	      struct pt_regs *regs)
{
	sigset_t set;
	struct signal_frame __user *frame;
	unsigned long oldspc = regs->spc;
	unsigned long oldccs = regs->ccs;

	frame = (struct signal_frame *) rdusp();

	if (((long)frame) & 3)
		goto badframe;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;

	if (__get_user(set.sig[0], &frame->sc.oldmask) ||
	    (_NSIG_WORDS > 1 && __copy_from_user(&set.sig[1],
						 frame->extramask,
						 sizeof(frame->extramask))))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);

	current->blocked = set;

	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	if (restore_sigcontext(regs, &frame->sc))
		goto badframe;

	keep_debug_flags(oldccs, oldspc, regs);

	return regs->r10;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

asmlinkage int
sys_rt_sigreturn(long r10, long r11, long r12, long r13, long mof, long srp,
		 struct pt_regs *regs)
{
	sigset_t set;
	struct rt_signal_frame __user *frame;
	unsigned long oldspc = regs->spc;
	unsigned long oldccs = regs->ccs;

	frame = (struct rt_signal_frame *) rdusp();

	if (((long)frame) & 3)
		goto badframe;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;

	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);

	current->blocked = set;

	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	if (restore_sigcontext(regs, &frame->uc.uc_mcontext))
		goto badframe;

	if (do_sigaltstack(&frame->uc.uc_stack, NULL, rdusp()) == -EFAULT)
		goto badframe;

	keep_debug_flags(oldccs, oldspc, regs);

	return regs->r10;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

static int
setup_sigcontext(struct sigcontext __user *sc, struct pt_regs *regs,
		 unsigned long mask)
{
	int err;
	unsigned long usp;

	err = 0;
	usp = rdusp();

	err |= __copy_to_user(sc, regs, sizeof(struct pt_regs));

	err |= __put_user(mask, &sc->oldmask);
	err |= __put_user(usp, &sc->usp);

	return err;
}

static inline void __user *
get_sigframe(struct k_sigaction *ka, struct pt_regs * regs, size_t frame_size)
{
	unsigned long sp;

	sp = rdusp();

	
	if (ka->sa.sa_flags & SA_ONSTACK) {
		if (!on_sig_stack(sp))
			sp = current->sas_ss_sp + current->sas_ss_size;
	}

	
	sp &= ~3;

	return (void __user *)(sp - frame_size);
}

static int
setup_frame(int sig, struct k_sigaction *ka,  sigset_t *set,
	    struct pt_regs * regs)
{
	int err;
	unsigned long return_ip;
	struct signal_frame __user *frame;

	err = 0;
	frame = get_sigframe(ka, regs, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	err |= setup_sigcontext(&frame->sc, regs, set->sig[0]);

	if (err)
		goto give_sigsegv;

	if (_NSIG_WORDS > 1) {
		err |= __copy_to_user(frame->extramask, &set->sig[1],
				      sizeof(frame->extramask));
	}

	if (err)
		goto give_sigsegv;

	if (ka->sa.sa_flags & SA_RESTORER) {
		return_ip = (unsigned long)ka->sa.sa_restorer;
	} else {
		
		return_ip = cris_signal_return_page;

		err |= __put_user(0x9c5f,         (short __user*)(frame->retcode+0));
		err |= __put_user(__NR_sigreturn, (short __user*)(frame->retcode+2));
		err |= __put_user(0xe93d,         (short __user*)(frame->retcode+4));
	}

	if (err)
		goto give_sigsegv;

	regs->erp = (unsigned long) ka->sa.sa_handler;
	regs->srp = return_ip;
	regs->r10 = sig;

	
	wrusp((unsigned long)frame);

	return 0;

give_sigsegv:
	if (sig == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;

	force_sig(SIGSEGV, current);
	return -EFAULT;
}

static int
setup_rt_frame(int sig, struct k_sigaction *ka, siginfo_t *info,
	       sigset_t *set, struct pt_regs * regs)
{
	int err;
	unsigned long return_ip;
	struct rt_signal_frame __user *frame;

	err = 0;
	frame = get_sigframe(ka, regs, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	

	err |= __put_user(&frame->info, &frame->pinfo);
	err |= __put_user(&frame->uc, &frame->puc);
	err |= copy_siginfo_to_user(&frame->info, info);

	if (err)
		goto give_sigsegv;

	
	err |= __clear_user(&frame->uc, offsetof(struct ucontext, uc_mcontext));
	err |= setup_sigcontext(&frame->uc.uc_mcontext, regs, set->sig[0]);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));

	if (err)
		goto give_sigsegv;

	if (ka->sa.sa_flags & SA_RESTORER) {
		return_ip = (unsigned long) ka->sa.sa_restorer;
	} else {
		
		return_ip = cris_signal_return_page + 6;

		err |= __put_user(0x9c5f, (short __user*)(frame->retcode+0));

		err |= __put_user(__NR_rt_sigreturn,
				  (short __user*)(frame->retcode+2));

		err |= __put_user(0xe93d, (short __user*)(frame->retcode+4));
	}

	if (err)
		goto give_sigsegv;

	regs->erp = (unsigned long) ka->sa.sa_handler;
	regs->srp = return_ip;
	regs->r10 = sig;
	regs->r11 = (unsigned long) &frame->info;
	regs->r12 = 0;

	
	wrusp((unsigned long)frame);

	return 0;

give_sigsegv:
	if (sig == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;

	force_sig(SIGSEGV, current);
	return -EFAULT;
}

static inline int
handle_signal(int canrestart, unsigned long sig,
	      siginfo_t *info, struct k_sigaction *ka,
              sigset_t *oldset, struct pt_regs * regs)
{
	int ret;

	
	if (canrestart) {
		
		switch (regs->r10) {
			case -ERESTART_RESTARTBLOCK:
			case -ERESTARTNOHAND:
				regs->r10 = -EINTR;
				break;

                        case -ERESTARTSYS:
				if (!(ka->sa.sa_flags & SA_RESTART)) {
					regs->r10 = -EINTR;
					break;
				}

				

			case -ERESTARTNOINTR:
				RESTART_CRIS_SYS(regs);
				break;
                }
        }

	
	if (ka->sa.sa_flags & SA_SIGINFO)
		ret = setup_rt_frame(sig, ka, info, oldset, regs);
	else
		ret = setup_frame(sig, ka, oldset, regs);

	if (ka->sa.sa_flags & SA_ONESHOT)
		ka->sa.sa_handler = SIG_DFL;

	if (ret == 0) {
		spin_lock_irq(&current->sighand->siglock);
		sigorsets(&current->blocked, &current->blocked,
			&ka->sa.sa_mask);
		if (!(ka->sa.sa_flags & SA_NODEFER))
			sigaddset(&current->blocked, sig);
		recalc_sigpending();
		spin_unlock_irq(&current->sighand->siglock);
	}

	return ret;
}

void
do_signal(int canrestart, struct pt_regs *regs)
{
	int signr;
	siginfo_t info;
        struct k_sigaction ka;
	sigset_t *oldset;

	if (!user_mode(regs))
		return;

	if (test_thread_flag(TIF_RESTORE_SIGMASK))
		oldset = &current->saved_sigmask;
	else
		oldset = &current->blocked;

	signr = get_signal_to_deliver(&info, &ka, regs, NULL);

	if (signr > 0) {
		
		if (handle_signal(canrestart, signr, &info, &ka,
				oldset, regs)) {
			if (test_thread_flag(TIF_RESTORE_SIGMASK))
				clear_thread_flag(TIF_RESTORE_SIGMASK);
		}

		return;
	}

	
	if (canrestart) {
		
		if (regs->r10 == -ERESTARTNOHAND ||
		    regs->r10 == -ERESTARTSYS ||
		    regs->r10 == -ERESTARTNOINTR) {
			RESTART_CRIS_SYS(regs);
		}

		if (regs->r10 == -ERESTART_RESTARTBLOCK){
			regs->r9 = __NR_restart_syscall;
			regs->erp -= 2;
		}
	}

	if (test_thread_flag(TIF_RESTORE_SIGMASK)) {
		clear_thread_flag(TIF_RESTORE_SIGMASK);
		sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
	}
}

asmlinkage void
ugdb_trap_user(struct thread_info *ti, int sig)
{
	if (((user_regs(ti)->exs & 0xff00) >> 8) != SINGLE_STEP_INTR_VECT) {
		user_regs(ti)->spc = 0;
	}
	if (((user_regs(ti)->exs & 0xff00) >> 8) == BREAK_8_INTR_VECT) {
		
		if (!(user_regs(ti)->erp & 0x1))
			user_regs(ti)->erp -= 2;
	}
	sys_kill(ti->task->pid, sig);
}

void
keep_debug_flags(unsigned long oldccs, unsigned long oldspc,
		 struct pt_regs *regs)
{
	if (oldccs & (1 << Q_CCS_BITNR)) {
		regs->ccs |= (1 << Q_CCS_BITNR);
		
		if (!(oldccs & (1 << (S_CCS_BITNR + CCS_SHIFT)))) {
			printk("Q flag but no S flag?");
		}
		regs->ccs |= (1 << (S_CCS_BITNR + CCS_SHIFT));
		
		regs->spc = oldspc;

	} else if (oldccs & (1 << (S_CCS_BITNR + CCS_SHIFT))) {
		regs->ccs |= (1 << (S_CCS_BITNR + CCS_SHIFT));
	} else if (regs->spc) {
		regs->spc = 0;
		regs->ccs &= ~(1 << (S_CCS_BITNR + CCS_SHIFT));
	}
}

int __init
cris_init_signal(void)
{
	u16* data = kmalloc(PAGE_SIZE, GFP_KERNEL);

	
	data[0] = 0x9c5f;
	data[1] = __NR_sigreturn;
	data[2] = 0xe93d;
	
	data[3] = 0x9c5f;
	data[4] = __NR_rt_sigreturn;
	data[5] = 0xe93d;

	
	cris_signal_return_page = (unsigned long)
          __ioremap_prot(virt_to_phys(data), PAGE_SIZE, PAGE_SIGNAL_TRAMPOLINE);

	return 0;
}

__initcall(cris_init_signal);
