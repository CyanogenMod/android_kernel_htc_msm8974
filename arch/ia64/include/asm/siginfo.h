#ifndef _ASM_IA64_SIGINFO_H
#define _ASM_IA64_SIGINFO_H


#define __ARCH_SI_PREAMBLE_SIZE	(4 * sizeof(int))

#define HAVE_ARCH_SIGINFO_T
#define HAVE_ARCH_COPY_SIGINFO
#define HAVE_ARCH_COPY_SIGINFO_TO_USER

#include <asm-generic/siginfo.h>

typedef struct siginfo {
	int si_signo;
	int si_errno;
	int si_code;
	int __pad0;

	union {
		int _pad[SI_PAD_SIZE];

		
		struct {
			pid_t _pid;		
			uid_t _uid;		
		} _kill;

		
		struct {
			timer_t _tid;		
			int _overrun;		
			char _pad[sizeof(__ARCH_SI_UID_T) - sizeof(int)];
			sigval_t _sigval;	
			int _sys_private;	
		} _timer;

		
		struct {
			pid_t _pid;		
			uid_t _uid;		
			sigval_t _sigval;
		} _rt;

		
		struct {
			pid_t _pid;		
			uid_t _uid;		
			int _status;		
			clock_t _utime;
			clock_t _stime;
		} _sigchld;

		
		struct {
			void __user *_addr;	
			int _imm;		
			unsigned int _flags;	
			unsigned long _isr;	
			short _addr_lsb;	
		} _sigfault;

		
		struct {
			long _band;	
			int _fd;
		} _sigpoll;
	} _sifields;
} siginfo_t;

#define si_imm		_sifields._sigfault._imm	
#define si_flags	_sifields._sigfault._flags
#define si_isr		_sifields._sigfault._isr

#define __ISR_VALID_BIT	0
#define __ISR_VALID	(1 << __ISR_VALID_BIT)

#define ILL_BADIADDR	(__SI_FAULT|9)	
#define __ILL_BREAK	(__SI_FAULT|10)	
#define __ILL_BNDMOD	(__SI_FAULT|11)	
#undef NSIGILL
#define NSIGILL		11

#define __FPE_DECOVF	(__SI_FAULT|9)	
#define __FPE_DECDIV	(__SI_FAULT|10)	
#define __FPE_DECERR	(__SI_FAULT|11)	
#define __FPE_INVASC	(__SI_FAULT|12)	
#define __FPE_INVDEC	(__SI_FAULT|13)	
#undef NSIGFPE
#define NSIGFPE		13

#define __SEGV_PSTKOVF	(__SI_FAULT|3)	
#undef NSIGSEGV
#define NSIGSEGV	3

#undef NSIGTRAP
#define NSIGTRAP	4

#ifdef __KERNEL__
#include <linux/string.h>

static inline void
copy_siginfo (siginfo_t *to, siginfo_t *from)
{
	if (from->si_code < 0)
		memcpy(to, from, sizeof(siginfo_t));
	else
		
		memcpy(to, from, 4*sizeof(int) + sizeof(from->_sifields._sigchld));
}

#endif 

#endif 
