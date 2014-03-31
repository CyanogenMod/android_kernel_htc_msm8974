/*
 * Hardware info about DECstation 5000/2x0 systems (otherwise known as
 * 3max+) and DECsystem 5900 systems (otherwise known as bigmax) which
 * differ mechanically but are otherwise identical (both are known as
 * KN03).
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995,1996 by Paul M. Antoine, some code and definitions
 * are by courtesy of Chris Fraser.
 * Copyright (C) 2000, 2002, 2003, 2005  Maciej W. Rozycki
 */
#ifndef __ASM_MIPS_DEC_KN03_H
#define __ASM_MIPS_DEC_KN03_H

#include <asm/dec/ecc.h>
#include <asm/dec/ioasic_addrs.h>

#define KN03_SLOT_BASE		0x1f800000

#define KN03_CPU_INR_HALT	6	
#define KN03_CPU_INR_BUS	5	
#define KN03_CPU_INR_RES_4	4	
#define KN03_CPU_INR_RTC	3	
#define KN03_CPU_INR_CASCADE	2	

#define KN03_IO_INR_3MAXP	15	
#define KN03_IO_INR_NVRAM	14	
#define KN03_IO_INR_TC2		13	
#define KN03_IO_INR_TC1		12	
#define KN03_IO_INR_TC0		11	
#define KN03_IO_INR_NRMOD	10	
#define KN03_IO_INR_ASC		9	
#define KN03_IO_INR_LANCE	8	
#define KN03_IO_INR_SCC1	7	
#define KN03_IO_INR_SCC0	6	
#define KN03_IO_INR_RTC		5	
#define KN03_IO_INR_PSU		4	
#define KN03_IO_INR_RES_3	3	
#define KN03_IO_INR_ASC_DATA	2	
#define KN03_IO_INR_PBNC	1	
#define KN03_IO_INR_PBNO	0	


#define KN03_MCR_RES_16		(0xffff<<16)	
#define KN03_MCR_DIAGCHK	(1<<15)		
#define KN03_MCR_DIAGGEN	(1<<14)		
#define KN03_MCR_CORRECT	(1<<13)		
#define KN03_MCR_RES_11		(0x3<<12)	
#define KN03_MCR_BNK32M		(1<<10)		
#define KN03_MCR_RES_7		(0x7<<7)	
#define KN03_MCR_CHECK		(0x7f<<0)	

#define KN03_IO_SSR_TXDIS1	(1<<14)		
#define KN03_IO_SSR_TXDIS0	(1<<13)		
#define KN03_IO_SSR_RES_12	(1<<12)		

#define KN03_IO_SSR_LEDS	(0xff<<0)	

#endif 
