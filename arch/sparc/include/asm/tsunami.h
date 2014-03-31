/*
 * tsunami.h:  Module specific definitions for Tsunami V8 Sparcs
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _SPARC_TSUNAMI_H
#define _SPARC_TSUNAMI_H

#include <asm/asi.h>


#define TSUNAMI_SW        0x00800000
#define TSUNAMI_AV        0x00400000
#define TSUNAMI_DV        0x00200000
#define TSUNAMI_MV        0x00100000
#define TSUNAMI_PC        0x00020000
#define TSUNAMI_ITD       0x00010000
#define TSUNAMI_ALC       0x00008000
#define TSUNAMI_PE        0x00001000
#define TSUNAMI_RCMASK    0x00000C00
#define TSUNAMI_IENAB     0x00000200
#define TSUNAMI_DENAB     0x00000100
#define TSUNAMI_NF        0x00000002
#define TSUNAMI_ME        0x00000001

static inline void tsunami_flush_icache(void)
{
	__asm__ __volatile__("sta %%g0, [%%g0] %0\n\t"
			     : 
			     : "i" (ASI_M_IC_FLCLEAR)
			     : "memory");
}

static inline void tsunami_flush_dcache(void)
{
	__asm__ __volatile__("sta %%g0, [%%g0] %0\n\t"
			     : 
			     : "i" (ASI_M_DC_FLCLEAR)
			     : "memory");
}

#endif 
