#ifndef _ASM_PARISC_SIGNAL_H
#define _ASM_PARISC_SIGNAL_H

#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGIOT		 6
#define SIGEMT		 7
#define SIGFPE		 8
#define SIGKILL		 9
#define SIGBUS		10
#define SIGSEGV		11
#define SIGSYS		12 
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGUSR1		16
#define SIGUSR2		17
#define SIGCHLD		18
#define SIGPWR		19
#define SIGVTALRM	20
#define SIGPROF		21
#define SIGIO		22
#define SIGPOLL		SIGIO
#define SIGWINCH	23
#define SIGSTOP		24
#define SIGTSTP		25
#define SIGCONT		26
#define SIGTTIN		27
#define SIGTTOU		28
#define SIGURG		29
#define SIGLOST		30 
#define	SIGUNUSED	31
#define SIGRESERVE	SIGUNUSED

#define SIGXCPU		33
#define SIGXFSZ		34
#define SIGSTKFLT	36

#define SIGRTMIN	37
#define SIGRTMAX	_NSIG 

#define SA_ONSTACK	0x00000001
#define SA_RESETHAND	0x00000004
#define SA_NOCLDSTOP	0x00000008
#define SA_SIGINFO	0x00000010
#define SA_NODEFER	0x00000020
#define SA_RESTART	0x00000040
#define SA_NOCLDWAIT	0x00000080
#define _SA_SIGGFAULT	0x00000100 

#define SA_NOMASK	SA_NODEFER
#define SA_ONESHOT	SA_RESETHAND

#define SA_RESTORER	0x04000000 

#define SS_ONSTACK	1
#define SS_DISABLE	2

#define MINSIGSTKSZ	2048
#define SIGSTKSZ	8192

#ifdef __KERNEL__

#define _NSIG		64
#define _NSIG_BPW	BITS_PER_LONG
#define _NSIG_WORDS	(_NSIG / _NSIG_BPW)

#endif 

#define SIG_BLOCK          0	
#define SIG_UNBLOCK        1	
#define SIG_SETMASK        2	

#define SIG_DFL	((__sighandler_t)0)	
#define SIG_IGN	((__sighandler_t)1)	
#define SIG_ERR	((__sighandler_t)-1)	

# ifndef __ASSEMBLY__

#  include <linux/types.h>

struct siginfo;

#ifdef CONFIG_64BIT
typedef char __user *__sighandler_t;
#else
typedef void __signalfn_t(int);
typedef __signalfn_t __user *__sighandler_t;
#endif

typedef struct sigaltstack {
	void __user *ss_sp;
	int ss_flags;
	size_t ss_size;
} stack_t;

#ifdef __KERNEL__


typedef unsigned long old_sigset_t;		

typedef struct {
	
	unsigned long sig[_NSIG_WORDS];
} sigset_t;

struct sigaction {
	__sighandler_t sa_handler;
	unsigned long sa_flags;
	sigset_t sa_mask;		
};

struct k_sigaction {
	struct sigaction sa;
};

#define ptrace_signal_deliver(regs, cookie) do { } while (0)

#include <asm/sigcontext.h>

#endif 
#endif 
#endif 
