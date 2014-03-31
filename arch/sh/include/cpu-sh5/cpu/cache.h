#ifndef __ASM_SH_CPU_SH5_CACHE_H
#define __ASM_SH_CPU_SH5_CACHE_H

/*
 * include/asm-sh/cpu-sh5/cache.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 * Copyright (C) 2003, 2004  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#define L1_CACHE_SHIFT		5

#define SH_CACHE_VALID		(1LL<<0)
#define SH_CACHE_UPDATED	(1LL<<57)

#define SH_CACHE_COMBINED	0
#define SH_CACHE_ASSOC		0

#define SH_CACHE_MODE_WT	(1LL<<0)
#define SH_CACHE_MODE_WB	(1LL<<1)

#define ICCR_BASE	0x01600000	
#define ICCR_REG0	0		
#define ICCR_REG1	1		
#define ICCR0		ICCR_BASE+ICCR_REG0
#define ICCR1		ICCR_BASE+ICCR_REG1

#define ICCR0_OFF	0x0		
#define ICCR0_ON	0x1		
#define ICCR0_ICI	0x2		

#define ICCR1_NOLOCK	0x0		

#define OCCR_BASE	0x01E00000	
#define OCCR_REG0	0		
#define OCCR_REG1	1		
#define OCCR0		OCCR_BASE+OCCR_REG0
#define OCCR1		OCCR_BASE+OCCR_REG1

#define OCCR0_OFF	0x0		
#define OCCR0_ON	0x1		
#define OCCR0_OCI	0x2		
#define OCCR0_WT	0x4		
#define OCCR0_WB	0x0		

#define OCCR1_NOLOCK	0x0		


#define CACHE_IC_ADDRESS_ARRAY 0x01000000

#define CACHE_OC_ADDRESS_ARRAY 0x01800000


#define CACHE_OC_N_SYNBITS  1               
#define CACHE_OC_SYN_SHIFT  12
#define CACHE_OC_SYN_MASK   (((1UL<<CACHE_OC_N_SYNBITS)-1)<<CACHE_OC_SYN_SHIFT)


#endif 
