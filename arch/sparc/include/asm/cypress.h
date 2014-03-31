/*
 * cypress.h: Cypress module specific definitions and defines.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _SPARC_CYPRESS_H
#define _SPARC_CYPRESS_H



#define CYPRESS_MCA       0x00c00000
#define CYPRESS_MCM       0x00300000
#define CYPRESS_MVALID    0x00080000
#define CYPRESS_MIDMASK   0x00078000   
#define CYPRESS_BMODE     0x00004000
#define CYPRESS_ACENABLE  0x00002000
#define CYPRESS_MRFLCT    0x00000800   
#define CYPRESS_CMODE     0x00000400
#define CYPRESS_CLOCK     0x00000200   
#define CYPRESS_CENABLE   0x00000100
#define CYPRESS_NFAULT    0x00000002
#define CYPRESS_MENABLE   0x00000001

static inline void cypress_flush_page(unsigned long page)
{
	__asm__ __volatile__("sta %%g0, [%0] %1\n\t" : :
			     "r" (page), "i" (ASI_M_FLUSH_PAGE));
}

static inline void cypress_flush_segment(unsigned long addr)
{
	__asm__ __volatile__("sta %%g0, [%0] %1\n\t" : :
			     "r" (addr), "i" (ASI_M_FLUSH_SEG));
}

static inline void cypress_flush_region(unsigned long addr)
{
	__asm__ __volatile__("sta %%g0, [%0] %1\n\t" : :
			     "r" (addr), "i" (ASI_M_FLUSH_REGION));
}

static inline void cypress_flush_context(void)
{
	__asm__ __volatile__("sta %%g0, [%%g0] %0\n\t" : :
			     "i" (ASI_M_FLUSH_CTX));
}


#endif 
