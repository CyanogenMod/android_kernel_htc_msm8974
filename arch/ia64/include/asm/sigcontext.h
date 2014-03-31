#ifndef _ASM_IA64_SIGCONTEXT_H
#define _ASM_IA64_SIGCONTEXT_H

/*
 * Copyright (C) 1998, 1999, 2001 Hewlett-Packard Co
 * Copyright (C) 1998, 1999, 2001 David Mosberger-Tang <davidm@hpl.hp.com>
 */

#include <asm/fpu.h>

#define IA64_SC_FLAG_ONSTACK_BIT		0	
#define IA64_SC_FLAG_IN_SYSCALL_BIT		1	
#define IA64_SC_FLAG_FPH_VALID_BIT		2	

#define IA64_SC_FLAG_ONSTACK		(1 << IA64_SC_FLAG_ONSTACK_BIT)
#define IA64_SC_FLAG_IN_SYSCALL		(1 << IA64_SC_FLAG_IN_SYSCALL_BIT)
#define IA64_SC_FLAG_FPH_VALID		(1 << IA64_SC_FLAG_FPH_VALID_BIT)

# ifndef __ASSEMBLY__


struct sigcontext {
	unsigned long		sc_flags;	
	unsigned long		sc_nat;		
	stack_t			sc_stack;	

	unsigned long		sc_ip;		
	unsigned long		sc_cfm;		
	unsigned long		sc_um;		
	unsigned long		sc_ar_rsc;	
	unsigned long		sc_ar_bsp;	
	unsigned long		sc_ar_rnat;	
	unsigned long		sc_ar_ccv;	
	unsigned long		sc_ar_unat;	
	unsigned long		sc_ar_fpsr;	
	unsigned long		sc_ar_pfs;	
	unsigned long		sc_ar_lc;	
	unsigned long		sc_pr;		
	unsigned long		sc_br[8];	
	
	unsigned long		sc_gr[32];	
	struct ia64_fpreg	sc_fr[128];	

	unsigned long		sc_rbs_base;	
	unsigned long		sc_loadrs;	

	unsigned long		sc_ar25;	
	unsigned long		sc_ar26;	
	unsigned long		sc_rsvd[12];	
	sigset_t		sc_mask;	
};

# endif 
#endif 
