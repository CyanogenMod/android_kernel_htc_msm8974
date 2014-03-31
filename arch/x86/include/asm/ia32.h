#ifndef _ASM_X86_IA32_H
#define _ASM_X86_IA32_H


#ifdef CONFIG_IA32_EMULATION

#include <linux/compat.h>


#include <asm/sigcontext32.h>

struct sigaction32 {
	unsigned int  sa_handler;	
	unsigned int sa_flags;
	unsigned int sa_restorer;	
	compat_sigset_t sa_mask;	
};

struct old_sigaction32 {
	unsigned int  sa_handler;	
	compat_old_sigset_t sa_mask;	
	unsigned int sa_flags;
	unsigned int sa_restorer;	
};

typedef struct sigaltstack_ia32 {
	unsigned int	ss_sp;
	int		ss_flags;
	unsigned int	ss_size;
} stack_ia32_t;

struct ucontext_ia32 {
	unsigned int	  uc_flags;
	unsigned int 	  uc_link;
	stack_ia32_t	  uc_stack;
	struct sigcontext_ia32 uc_mcontext;
	compat_sigset_t	  uc_sigmask;	
};

struct ucontext_x32 {
	unsigned int	  uc_flags;
	unsigned int 	  uc_link;
	stack_ia32_t	  uc_stack;
	unsigned int	  uc__pad0;     
	struct sigcontext uc_mcontext;  
	compat_sigset_t	  uc_sigmask;	
};

struct stat64 {
	unsigned long long	st_dev;
	unsigned char		__pad0[4];

#define STAT64_HAS_BROKEN_ST_INO	1
	unsigned int		__st_ino;

	unsigned int		st_mode;
	unsigned int		st_nlink;

	unsigned int		st_uid;
	unsigned int		st_gid;

	unsigned long long	st_rdev;
	unsigned char		__pad3[4];

	long long		st_size;
	unsigned int		st_blksize;

	long long		st_blocks;

	unsigned 		st_atime;
	unsigned 		st_atime_nsec;
	unsigned 		st_mtime;
	unsigned 		st_mtime_nsec;
	unsigned 		st_ctime;
	unsigned 		st_ctime_nsec;

	unsigned long long	st_ino;
} __attribute__((packed));

typedef struct compat_siginfo {
	int si_signo;
	int si_errno;
	int si_code;

	union {
		int _pad[((128 / sizeof(int)) - 3)];

		
		struct {
			unsigned int _pid;	
			unsigned int _uid;	
		} _kill;

		
		struct {
			compat_timer_t _tid;	
			int _overrun;		
			compat_sigval_t _sigval;	
			int _sys_private;	
			int _overrun_incr;	
		} _timer;

		
		struct {
			unsigned int _pid;	
			unsigned int _uid;	
			compat_sigval_t _sigval;
		} _rt;

		
		struct {
			unsigned int _pid;	
			unsigned int _uid;	
			int _status;		
			compat_clock_t _utime;
			compat_clock_t _stime;
		} _sigchld;

		
		struct {
			unsigned int _pid;	
			unsigned int _uid;	
			int _status;		
			compat_s64 _utime;
			compat_s64 _stime;
		} _sigchld_x32;

		
		struct {
			unsigned int _addr;	
		} _sigfault;

		
		struct {
			int _band;	
			int _fd;
		} _sigpoll;
	} _sifields;
} compat_siginfo_t;

#define IA32_STACK_TOP IA32_PAGE_OFFSET

#ifdef __KERNEL__
struct linux_binprm;
extern int ia32_setup_arg_pages(struct linux_binprm *bprm,
				unsigned long stack_top, int exec_stack);
struct mm_struct;
extern void ia32_pick_mmap_layout(struct mm_struct *mm);

#endif

#endif 

#endif 
