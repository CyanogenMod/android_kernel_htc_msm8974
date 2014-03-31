#ifndef _ASM_PARISC_COMPAT_UCONTEXT_H
#define _ASM_PARISC_COMPAT_UCONTEXT_H

#include <linux/compat.h>

struct compat_ucontext {
	compat_uint_t uc_flags;
	compat_uptr_t uc_link;
	compat_stack_t uc_stack;		
	
	compat_uint_t pad[1];
	struct compat_sigcontext uc_mcontext;
	compat_sigset_t uc_sigmask;	
};

#endif 
