#ifndef _ASM_IA64_FPU_H
#define _ASM_IA64_FPU_H

/*
 * Copyright (C) 1998, 1999, 2002, 2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */

#include <linux/types.h>

#define FPSR_TRAP_VD	(1 << 0)	
#define FPSR_TRAP_DD	(1 << 1)	
#define FPSR_TRAP_ZD	(1 << 2)	
#define FPSR_TRAP_OD	(1 << 3)	
#define FPSR_TRAP_UD	(1 << 4)	
#define FPSR_TRAP_ID	(1 << 5)	
#define FPSR_S0(x)	((x) <<  6)
#define FPSR_S1(x)	((x) << 19)
#define FPSR_S2(x)	(__IA64_UL(x) << 32)
#define FPSR_S3(x)	(__IA64_UL(x) << 45)

#define FPSF_FTZ	(1 << 0)		
#define FPSF_WRE	(1 << 1)		
#define FPSF_PC(x)	(((x) & 0x3) << 2)	
#define FPSF_RC(x)	(((x) & 0x3) << 4)	
#define FPSF_TD		(1 << 6)		

#define FPSF_V		(1 <<  7)		
#define FPSF_D		(1 <<  8)		
#define FPSF_Z		(1 <<  9)		
#define FPSF_O		(1 << 10)		
#define FPSF_U		(1 << 11)		
#define FPSF_I		(1 << 12)		

#define FPRC_NEAREST	0x0
#define FPRC_NEGINF	0x1
#define FPRC_POSINF	0x2
#define FPRC_TRUNC	0x3

#define FPSF_DEFAULT	(FPSF_PC (0x3) | FPSF_RC (FPRC_NEAREST))

#define FPSR_DEFAULT	(FPSR_TRAP_VD | FPSR_TRAP_DD | FPSR_TRAP_ZD	\
			 | FPSR_TRAP_OD | FPSR_TRAP_UD | FPSR_TRAP_ID	\
			 | FPSR_S0 (FPSF_DEFAULT)			\
			 | FPSR_S1 (FPSF_DEFAULT | FPSF_TD | FPSF_WRE)	\
			 | FPSR_S2 (FPSF_DEFAULT | FPSF_TD)		\
			 | FPSR_S3 (FPSF_DEFAULT | FPSF_TD))

# ifndef __ASSEMBLY__

struct ia64_fpreg {
	union {
		unsigned long bits[2];
		long double __dummy;	
	} u;
};

# endif 

#endif 
