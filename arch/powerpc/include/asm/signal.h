#ifndef _ASM_POWERPC_SIGNAL_H
#define _ASM_POWERPC_SIGNAL_H

#include <linux/types.h>

#define _NSIG		64
#ifdef __powerpc64__
#define _NSIG_BPW	64
#else
#define _NSIG_BPW	32
#endif
#define _NSIG_WORDS	(_NSIG / _NSIG_BPW)

typedef unsigned long old_sigset_t;		

typedef struct {
	unsigned long sig[_NSIG_WORDS];
} sigset_t;

#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGIOT		 6
#define SIGBUS		 7
#define SIGFPE		 8
#define SIGKILL		 9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGSTKFLT	16
#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGTTIN		21
#define SIGTTOU		22
#define SIGURG		23
#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGWINCH	28
#define SIGIO		29
#define SIGPOLL		SIGIO
#define SIGPWR		30
#define SIGSYS		31
#define	SIGUNUSED	31

#define SIGRTMIN	32
#define SIGRTMAX	_NSIG

#define SA_NOCLDSTOP	0x00000001U
#define SA_NOCLDWAIT	0x00000002U
#define SA_SIGINFO	0x00000004U
#define SA_ONSTACK	0x08000000U
#define SA_RESTART	0x10000000U
#define SA_NODEFER	0x40000000U
#define SA_RESETHAND	0x80000000U

#define SA_NOMASK	SA_NODEFER
#define SA_ONESHOT	SA_RESETHAND

#define SA_RESTORER	0x04000000U

#define SS_ONSTACK	1
#define SS_DISABLE	2

#define MINSIGSTKSZ	2048
#define SIGSTKSZ	8192

#include <asm-generic/signal-defs.h>

struct old_sigaction {
	__sighandler_t sa_handler;
	old_sigset_t sa_mask;
	unsigned long sa_flags;
	__sigrestore_t sa_restorer;
};

struct sigaction {
	__sighandler_t sa_handler;
	unsigned long sa_flags;
	__sigrestore_t sa_restorer;
	sigset_t sa_mask;		
};

struct k_sigaction {
	struct sigaction sa;
};

typedef struct sigaltstack {
	void __user *ss_sp;
	int ss_flags;
	size_t ss_size;
} stack_t;

#ifdef __KERNEL__
struct pt_regs;
#define ptrace_signal_deliver(regs, cookie) do { } while (0)
#endif 

#ifndef __powerpc64__
struct sig_dbg_op {
	int dbg_type;
	unsigned long dbg_value;
};

#define SIG_DBG_SINGLE_STEPPING		1

#define SIG_DBG_BRANCH_TRACING		2
#endif 

#endif 
