/*
 * Copyright (C) 2004 PathScale, Inc
 * Copyright (C) 2004 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <strings.h>
#include "as-layout.h"
#include "kern_util.h"
#include "os.h"
#include "sysdep/mcontext.h"

void (*sig_info[NSIG])(int, struct uml_pt_regs *) = {
	[SIGTRAP]	= relay_signal,
	[SIGFPE]	= relay_signal,
	[SIGILL]	= relay_signal,
	[SIGWINCH]	= winch,
	[SIGBUS]	= bus_handler,
	[SIGSEGV]	= segv_handler,
	[SIGIO]		= sigio_handler,
	[SIGVTALRM]	= timer_handler };

static void sig_handler_common(int sig, mcontext_t *mc)
{
	struct uml_pt_regs r;
	int save_errno = errno;

	r.is_user = 0;
	if (sig == SIGSEGV) {
		
		get_regs_from_mc(&r, mc);
		GET_FAULTINFO_FROM_MC(r.faultinfo, mc);
	}

	
	if ((sig != SIGIO) && (sig != SIGWINCH) && (sig != SIGVTALRM))
		unblock_signals();

	(*sig_info[sig])(sig, &r);

	errno = save_errno;
}

#define SIGIO_BIT 0
#define SIGIO_MASK (1 << SIGIO_BIT)

#define SIGVTALRM_BIT 1
#define SIGVTALRM_MASK (1 << SIGVTALRM_BIT)

static int signals_enabled;
static unsigned int signals_pending;

void sig_handler(int sig, mcontext_t *mc)
{
	int enabled;

	enabled = signals_enabled;
	if (!enabled && (sig == SIGIO)) {
		signals_pending |= SIGIO_MASK;
		return;
	}

	block_signals();

	sig_handler_common(sig, mc);

	set_signals(enabled);
}

static void real_alarm_handler(mcontext_t *mc)
{
	struct uml_pt_regs regs;

	if (mc != NULL)
		get_regs_from_mc(&regs, mc);
	regs.is_user = 0;
	unblock_signals();
	timer_handler(SIGVTALRM, &regs);
}

void alarm_handler(int sig, mcontext_t *mc)
{
	int enabled;

	enabled = signals_enabled;
	if (!signals_enabled) {
		signals_pending |= SIGVTALRM_MASK;
		return;
	}

	block_signals();

	real_alarm_handler(mc);
	set_signals(enabled);
}

void timer_init(void)
{
	set_handler(SIGVTALRM);
}

void set_sigstack(void *sig_stack, int size)
{
	stack_t stack = ((stack_t) { .ss_flags	= 0,
				     .ss_sp	= (__ptr_t) sig_stack,
				     .ss_size 	= size - sizeof(void *) });

	if (sigaltstack(&stack, NULL) != 0)
		panic("enabling signal stack failed, errno = %d\n", errno);
}

static void (*handlers[_NSIG])(int sig, mcontext_t *mc) = {
	[SIGSEGV] = sig_handler,
	[SIGBUS] = sig_handler,
	[SIGILL] = sig_handler,
	[SIGFPE] = sig_handler,
	[SIGTRAP] = sig_handler,

	[SIGIO] = sig_handler,
	[SIGWINCH] = sig_handler,
	[SIGVTALRM] = alarm_handler
};


static void hard_handler(int sig, siginfo_t *info, void *p)
{
	struct ucontext *uc = p;
	mcontext_t *mc = &uc->uc_mcontext;
	unsigned long pending = 1UL << sig;

	do {
		int nested, bail;

		bail = to_irq_stack(&pending);
		if (bail)
			return;

		nested = pending & 1;
		pending &= ~1;

		while ((sig = ffs(pending)) != 0){
			sig--;
			pending &= ~(1 << sig);
			(*handlers[sig])(sig, mc);
		}

		if (!nested)
			pending = from_irq_stack(nested);
	} while (pending);
}

void set_handler(int sig)
{
	struct sigaction action;
	int flags = SA_SIGINFO | SA_ONSTACK;
	sigset_t sig_mask;

	action.sa_sigaction = hard_handler;

	
	sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGVTALRM);
	sigaddset(&action.sa_mask, SIGIO);
	sigaddset(&action.sa_mask, SIGWINCH);

	if (sig == SIGSEGV)
		flags |= SA_NODEFER;

	if (sigismember(&action.sa_mask, sig))
		flags |= SA_RESTART; 

	action.sa_flags = flags;
	action.sa_restorer = NULL;
	if (sigaction(sig, &action, NULL) < 0)
		panic("sigaction failed - errno = %d\n", errno);

	sigemptyset(&sig_mask);
	sigaddset(&sig_mask, sig);
	if (sigprocmask(SIG_UNBLOCK, &sig_mask, NULL) < 0)
		panic("sigprocmask failed - errno = %d\n", errno);
}

int change_sig(int signal, int on)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, signal);
	if (sigprocmask(on ? SIG_UNBLOCK : SIG_BLOCK, &sigset, NULL) < 0)
		return -errno;

	return 0;
}

void block_signals(void)
{
	signals_enabled = 0;
	barrier();
}

void unblock_signals(void)
{
	int save_pending;

	if (signals_enabled == 1)
		return;

	while (1) {
		signals_enabled = 1;

		barrier();

		save_pending = signals_pending;
		if (save_pending == 0)
			return;

		signals_pending = 0;


		signals_enabled = 0;

		if (save_pending & SIGIO_MASK)
			sig_handler_common(SIGIO, NULL);

		if (save_pending & SIGVTALRM_MASK)
			real_alarm_handler(NULL);
	}
}

int get_signals(void)
{
	return signals_enabled;
}

int set_signals(int enable)
{
	int ret;
	if (signals_enabled == enable)
		return enable;

	ret = signals_enabled;
	if (enable)
		unblock_signals();
	else block_signals();

	return ret;
}
