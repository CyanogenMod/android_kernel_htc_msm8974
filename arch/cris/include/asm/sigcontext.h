
#ifndef _ASM_CRIS_SIGCONTEXT_H
#define _ASM_CRIS_SIGCONTEXT_H

#include <asm/ptrace.h>


struct sigcontext {
	struct pt_regs regs;  
	unsigned long oldmask;
	unsigned long usp;    
};

#endif

