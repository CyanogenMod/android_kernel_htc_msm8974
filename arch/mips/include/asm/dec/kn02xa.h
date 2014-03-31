/*
 * Hardware info common to DECstation 5000/1xx systems (otherwise
 * known as 3min or kn02ba) and Personal DECstations 5000/xx ones
 * (otherwise known as maxine or kn02ca).
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995,1996 by Paul M. Antoine, some code and definitions
 * are by courtesy of Chris Fraser.
 * Copyright (C) 2000, 2002, 2003, 2005  Maciej W. Rozycki
 *
 * These are addresses which have to be known early in the boot process.
 * For other addresses refer to tc.h, ioasic_addrs.h and friends.
 */
#ifndef __ASM_MIPS_DEC_KN02XA_H
#define __ASM_MIPS_DEC_KN02XA_H

#include <asm/dec/ioasic_addrs.h>

#define KN02XA_SLOT_BASE	0x1c000000

#define KN02XA_MER		0x0c400000	
#define KN02XA_MSR		0x0c800000	

#define KN02XA_MEM_CONF		0x0e000000	
#define KN02XA_EAR		0x0e000004	
#define KN02XA_BOOT0		0x0e000008	
#define KN02XA_MEM_INTR		0x0e00000c	

#define KN02XA_MER_RES_28	(0xf<<28)	
#define KN02XA_MER_RES_17	(0x3ff<<17)	
#define KN02XA_MER_PAGERR	(1<<16)		
#define KN02XA_MER_TRANSERR	(1<<15)		
#define KN02XA_MER_PARDIS	(1<<14)		
#define KN02XA_MER_SIZE		(1<<13)		
#define KN02XA_MER_RES_12	(1<<12)		
#define KN02XA_MER_BYTERR	(0xf<<8)	
#define KN02XA_MER_BYTERR_3	(0x8<<8)	
#define KN02XA_MER_BYTERR_2	(0x4<<8)	
#define KN02XA_MER_BYTERR_1	(0x2<<8)	
#define KN02XA_MER_BYTERR_0	(0x1<<8)	
#define KN02XA_MER_RES_0	(0xff<<0)	

#define KN02XA_MSR_RES_27	(0x1f<<27)	
#define KN02XA_MSR_RES_14	(0x7<<14)	
#define KN02XA_MSR_SIZE		(1<<13)		
#define KN02XA_MSR_RES_0	(0x1fff<<0)	

#define KN02XA_EAR_RES_29	(0x7<<29)	
#define KN02XA_EAR_ADDRESS	(0x7ffffff<<2)	
#define KN02XA_EAR_RES_0	(0x3<<0)	


#ifndef __ASSEMBLY__

#include <linux/interrupt.h>

struct pt_regs;

extern void dec_kn02xa_be_init(void);
extern int dec_kn02xa_be_handler(struct pt_regs *regs, int is_fixup);
extern irqreturn_t dec_kn02xa_be_interrupt(int irq, void *dev_id);
#endif

#endif 
