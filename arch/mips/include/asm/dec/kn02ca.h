/*
 *	include/asm-mips/dec/kn02ca.h
 *
 *	Personal DECstation 5000/xx (Maxine or KN02-CA) definitions.
 *
 *	Copyright (C) 2002, 2003  Maciej W. Rozycki
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */
#ifndef __ASM_MIPS_DEC_KN02CA_H
#define __ASM_MIPS_DEC_KN02CA_H

#include <asm/dec/kn02xa.h>		

#define KN02CA_CPU_INR_HALT	6	
#define KN02CA_CPU_INR_CASCADE	5	
#define KN02CA_CPU_INR_BUS	4	
#define KN02CA_CPU_INR_RTC	3	
#define KN02CA_CPU_INR_TIMER	2	

#define KN02CA_IO_INR_FLOPPY	15	
#define KN02CA_IO_INR_NVRAM	14	
#define KN02CA_IO_INR_POWERON	13	
#define KN02CA_IO_INR_TC0	12	
#define KN02CA_IO_INR_TIMER	12	
#define KN02CA_IO_INR_ISDN	11	
#define KN02CA_IO_INR_NRMOD	10	
#define KN02CA_IO_INR_ASC	9	
#define KN02CA_IO_INR_LANCE	8	
#define KN02CA_IO_INR_HDFLOPPY	7	
#define KN02CA_IO_INR_SCC0	6	
#define KN02CA_IO_INR_TC1	5	
#define KN02CA_IO_INR_XDFLOPPY	4	
#define KN02CA_IO_INR_VIDEO	3	
#define KN02CA_IO_INR_XVIDEO	2	
#define KN02CA_IO_INR_AB_XMIT	1	
#define KN02CA_IO_INR_AB_RECV	0	


#define KN02CA_MER_INTR		(1<<27)		

#define KN02CA_MSR_INTREN	(1<<26)		
#define KN02CA_MSR_MS10EN	(1<<25)		
#define KN02CA_MSR_PFORCE	(0xf<<21)	
#define KN02CA_MSR_MABEN	(1<<20)		
#define KN02CA_MSR_LASTBANK	(0x7<<17)	

#define KN03CA_IO_SSR_RES_14	(1<<14)		
#define KN03CA_IO_SSR_RES_13	(1<<13)		
#define KN03CA_IO_SSR_ISDN_RST	(1<<12)		

#define KN03CA_IO_SSR_FLOPPY_RST (1<<7)		
#define KN03CA_IO_SSR_VIDEO_RST	(1<<6)		
#define KN03CA_IO_SSR_AB_RST	(1<<5)		
#define KN03CA_IO_SSR_RES_4	(1<<4)		
#define KN03CA_IO_SSR_RES_3	(1<<4)		
#define KN03CA_IO_SSR_RES_2	(1<<2)		
#define KN03CA_IO_SSR_RES_1	(1<<1)		
#define KN03CA_IO_SSR_LED	(1<<0)		

#endif 
