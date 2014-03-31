/*
 * sbi.h:  SBI (Sbus Interface on sun4d) definitions
 *
 * Copyright (C) 1997 Jakub Jelinek <jj@sunsite.mff.cuni.cz>
 */

#ifndef _SPARC_SBI_H
#define _SPARC_SBI_H

#include <asm/obio.h>

struct sbi_regs {
	u32		cid;		
	u32		ctl;		
	u32		status;		
		u32		_unused1;
		
	u32		cfg0;		
	u32		cfg1;		
	u32		cfg2;		
	u32		cfg3;		

	u32		stb0;		
	u32		stb1;		
	u32		stb2;		
	u32		stb3;		

	u32		intr_state;	
	u32		intr_tid;	
	u32		intr_diag;	
};

#define SBI_CID			0x02800000
#define SBI_CTL			0x02800004
#define SBI_STATUS		0x02800008
#define SBI_CFG0		0x02800010
#define SBI_CFG1		0x02800014
#define SBI_CFG2		0x02800018
#define SBI_CFG3		0x0280001c
#define SBI_STB0		0x02800020
#define SBI_STB1		0x02800024
#define SBI_STB2		0x02800028
#define SBI_STB3		0x0280002c
#define SBI_INTR_STATE		0x02800030
#define SBI_INTR_TID		0x02800034
#define SBI_INTR_DIAG		0x02800038

#define SBI_CFG_BURST_MASK	0x0000001e

#define SBI2DEVID(sbino) ((sbino<<4)|2)



#ifndef __ASSEMBLY__

static inline int acquire_sbi(int devid, int mask)
{
	__asm__ __volatile__ ("swapa [%2] %3, %0" :
			      "=r" (mask) :
			      "0" (mask),
			      "r" (ECSR_DEV_BASE(devid) | SBI_INTR_STATE),
			      "i" (ASI_M_CTL));
	return mask;
}

static inline void release_sbi(int devid, int mask)
{
	__asm__ __volatile__ ("sta %0, [%1] %2" : :
			      "r" (mask),
			      "r" (ECSR_DEV_BASE(devid) | SBI_INTR_STATE),
			      "i" (ASI_M_CTL));
}

static inline void set_sbi_tid(int devid, int targetid)
{
	__asm__ __volatile__ ("sta %0, [%1] %2" : :
			      "r" (targetid),
			      "r" (ECSR_DEV_BASE(devid) | SBI_INTR_TID),
			      "i" (ASI_M_CTL));
}

static inline int get_sbi_ctl(int devid, int cfgno)
{
	int cfg;
	
	__asm__ __volatile__ ("lda [%1] %2, %0" :
			      "=r" (cfg) :
			      "r" ((ECSR_DEV_BASE(devid) | SBI_CFG0) + (cfgno<<2)),
			      "i" (ASI_M_CTL));
	return cfg;
}

static inline void set_sbi_ctl(int devid, int cfgno, int cfg)
{
	__asm__ __volatile__ ("sta %0, [%1] %2" : :
			      "r" (cfg),
			      "r" ((ECSR_DEV_BASE(devid) | SBI_CFG0) + (cfgno<<2)),
			      "i" (ASI_M_CTL));
}

#endif 

#endif 
