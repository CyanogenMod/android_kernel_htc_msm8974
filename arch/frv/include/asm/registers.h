/* registers.h: register frame declarations
 *
 * Copyright (C) 2003 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */


#ifndef _ASM_REGISTERS_H
#define _ASM_REGISTERS_H

#ifndef __ASSEMBLY__
#define __OFFSET(X,N)	((X)+(N)*4)
#define __OFFSETC(X,N)	xxxxxxxxxxxxxxxxxxxxxxxx
#else
#define __OFFSET(X,N)	((X)+(N)*4)
#define __OFFSETC(X,N)	((X)+(N))
#endif

#ifndef __ASSEMBLY__

struct pt_regs {
	unsigned long		psr;		
	unsigned long		isr;		
	unsigned long		ccr;		
	unsigned long		cccr;		
	unsigned long		lr;		
	unsigned long		lcr;		
	unsigned long		pc;		
	unsigned long		__status;	
	unsigned long		syscallno;	
	unsigned long		orig_gr8;	
	unsigned long		gner0;
	unsigned long		gner1;
	unsigned long long	iacc0;
	unsigned long		tbr;		
	unsigned long		sp;		
	unsigned long		fp;		
	unsigned long		gr3;
	unsigned long		gr4;
	unsigned long		gr5;
	unsigned long		gr6;
	unsigned long		gr7;		
	unsigned long		gr8;		
	unsigned long		gr9;		
	unsigned long		gr10;		
	unsigned long		gr11;		
	unsigned long		gr12;		
	unsigned long		gr13;		
	unsigned long		gr14;
	unsigned long		gr15;
	unsigned long		gr16;		
	unsigned long		gr17;		
	unsigned long		gr18;		
	unsigned long		gr19;
	unsigned long		gr20;
	unsigned long		gr21;
	unsigned long		gr22;
	unsigned long		gr23;
	unsigned long		gr24;
	unsigned long		gr25;
	unsigned long		gr26;
	unsigned long		gr27;
	struct pt_regs		*next_frame;	
	unsigned long		gr29;		
	unsigned long		gr30;		
	unsigned long		gr31;		
} __attribute__((aligned(8)));

#endif

#define REG__STATUS_STEP	0x00000001	
#define REG__STATUS_STEPPED	0x00000002	
#define REG__STATUS_BROKE	0x00000004	
#define REG__STATUS_SYSC_ENTRY	0x40000000	
#define REG__STATUS_SYSC_EXIT	0x80000000	

#define REG_GR(R)	__OFFSET(REG_GR0, (R))

#define REG_SP		REG_GR(1)
#define REG_FP		REG_GR(2)
#define REG_PREV_FRAME	REG_GR(28)	
#define REG_CURR_TASK	REG_GR(29)	

#ifndef __ASSEMBLY__

struct frv_debug_regs
{
	unsigned long		dcr;
	unsigned long		ibar[4] __attribute__((aligned(8)));
	unsigned long		dbar[4] __attribute__((aligned(8)));
	unsigned long		dbdr[4][4] __attribute__((aligned(8)));
	unsigned long		dbmr[4][4] __attribute__((aligned(8)));
} __attribute__((aligned(8)));

#endif

#ifndef __ASSEMBLY__

struct user_int_regs
{
	unsigned long		psr;		
	unsigned long		isr;		
	unsigned long		ccr;		
	unsigned long		cccr;		
	unsigned long		lr;		
	unsigned long		lcr;		
	unsigned long		pc;		
	unsigned long		__status;	
	unsigned long		syscallno;	
	unsigned long		orig_gr8;	
	unsigned long		gner[2];
	unsigned long long	iacc[1];

	union {
		unsigned long	tbr;
		unsigned long	gr[64];
	};
};

struct user_fpmedia_regs
{
	
	unsigned long	fr[64];
	unsigned long	fner[2];
	unsigned long	msr[2];
	unsigned long	acc[8];
	unsigned char	accg[8];
	unsigned long	fsr[1];
};

struct user_context
{
	struct user_int_regs		i;
	struct user_fpmedia_regs	f;

	void *extension;
} __attribute__((aligned(8)));

struct frv_frame0 {
	union {
		struct pt_regs		regs;
		struct user_context	uc;
	};

	struct frv_debug_regs		debug;

} __attribute__((aligned(32)));

#endif

#define __INT_GR(R)		__OFFSET(__INT_GR0,		(R))

#define __FPMEDIA_FR(R)		__OFFSET(__FPMEDIA_FR0,		(R))
#define __FPMEDIA_FNER(R)	__OFFSET(__FPMEDIA_FNER0,	(R))
#define __FPMEDIA_MSR(R)	__OFFSET(__FPMEDIA_MSR0,	(R))
#define __FPMEDIA_ACC(R)	__OFFSET(__FPMEDIA_ACC0,	(R))
#define __FPMEDIA_ACCG(R)	__OFFSETC(__FPMEDIA_ACCG0,	(R))
#define __FPMEDIA_FSR(R)	__OFFSET(__FPMEDIA_FSR0,	(R))

#define __THREAD_GR(R)		__OFFSET(__THREAD_GR16,		(R) - 16)

#endif 
