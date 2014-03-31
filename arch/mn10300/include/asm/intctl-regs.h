/* MN10300 On-board interrupt controller registers
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _ASM_INTCTL_REGS_H
#define _ASM_INTCTL_REGS_H

#include <asm/cpu-regs.h>

#ifdef __KERNEL__

#define GxICR(X)						\
	__SYSREG(0xd4000000 + (X) * 4 +				\
		 (((X) >= 64) && ((X) < 192)) * 0xf00, u16)

#define GxICR_u8(X)							\
	__SYSREG(0xd4000000 + (X) * 4 +					\
		 (((X) >= 64) && ((X) < 192)) * 0xf00, u8)

#include <proc/intctl-regs.h>

#define XIRQ_TRIGGER_LOWLEVEL	0
#define XIRQ_TRIGGER_HILEVEL	1
#define XIRQ_TRIGGER_NEGEDGE	2
#define XIRQ_TRIGGER_POSEDGE	3

#define NMIIRQ			0
#define NMICR			GxICR(NMIIRQ)	
#define NMICR_NMIF		0x0001		
#define NMICR_WDIF		0x0002		
#define NMICR_ABUSERR		0x0008		

#define GxICR_DETECT		0x0001		
#define GxICR_REQUEST		0x0010		
#define GxICR_ENABLE		0x0100		
#define GxICR_LEVEL		0x7000		
#define GxICR_LEVEL_0		0x0000		
#define GxICR_LEVEL_1		0x1000		
#define GxICR_LEVEL_2		0x2000		
#define GxICR_LEVEL_3		0x3000		
#define GxICR_LEVEL_4		0x4000		
#define GxICR_LEVEL_5		0x5000		
#define GxICR_LEVEL_6		0x6000		
#define GxICR_LEVEL_SHIFT	12
#define GxICR_NMI		0x8000		

#define NUM2GxICR_LEVEL(num)	((num) << GxICR_LEVEL_SHIFT)

#ifndef __ASSEMBLY__
extern void set_intr_level(int irq, u16 level);
extern void mn10300_set_lateack_irq_type(int irq);
#endif

#define XIRQxICR(X)		GxICR((X))	

#endif 

#endif 
