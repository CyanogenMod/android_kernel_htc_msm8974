/*
 * arch/arm/mach-ks8695/include/mach/regs-mem.h
 *
 * Copyright (C) 2006 Andrew Victor
 *
 * KS8695 - Memory Controller registers and bit definitions
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef KS8695_MEM_H
#define KS8695_MEM_H

#define KS8695_MEM_OFFSET	(0xF0000 + 0x4000)
#define KS8695_MEM_VA		(KS8695_IO_VA + KS8695_MEM_OFFSET)
#define KS8695_MEM_PA		(KS8695_IO_PA + KS8695_MEM_OFFSET)


#define KS8695_EXTACON0		(0x00)		
#define KS8695_EXTACON1		(0x04)		
#define KS8695_EXTACON2		(0x08)		
#define KS8695_ROMCON0		(0x10)		
#define KS8695_ROMCON1		(0x14)		
#define KS8695_ERGCON		(0x20)		
#define KS8695_SDCON0		(0x30)		
#define KS8695_SDCON1		(0x34)		
#define KS8695_SDGCON		(0x38)		
#define KS8695_SDBCON		(0x3c)		
#define KS8695_REFTIM		(0x40)		


#define EXTACON_EBNPTR		(0x3ff << 22)		
#define EXTACON_EBBPTR		(0x3ff << 12)		
#define EXTACON_EBTACT		(7     <<  9)		
#define EXTACON_EBTCOH		(7     <<  6)		
#define EXTACON_EBTACS		(7     <<  3)		
#define EXTACON_EBTCOS		(7     <<  0)		

#define ROMCON_RBNPTR		(0x3ff << 22)		
#define ROMCON_RBBPTR		(0x3ff << 12)		
#define ROMCON_RBTACC		(7     <<  4)		
#define ROMCON_RBTPA		(3     <<  2)		
#define ROMCON_PMC		(3     <<  0)		
#define		PMC_NORMAL		(0 << 0)
#define		PMC_4WORD		(1 << 0)
#define		PMC_8WORD		(2 << 0)
#define		PMC_16WORD		(3 << 0)

#define ERGCON_TMULT		(3 << 28)		
#define ERGCON_DSX2		(3 << 20)		
#define ERGCON_DSX1		(3 << 18)		
#define ERGCON_DSX0		(3 << 16)		
#define ERGCON_DSR1		(3 <<  2)		
#define ERGCON_DSR0		(3 <<  0)		

#define SDCON_DBNPTR		(0x3ff << 22)		
#define SDCON_DBBPTR		(0x3ff << 12)		
#define SDCON_DBCAB		(3     <<  8)		
#define SDCON_DBBNUM		(1     <<  3)		
#define SDCON_DBDBW		(3     <<  1)		

#define SDGCON_SDTRC		(3 << 2)		
#define SDGCON_SDCAS		(3 << 0)		

#define SDBCON_SDESTA		(1 << 31)		
#define SDBCON_RBUFBDIS		(1 << 24)		
#define SDBCON_WFIFOEN		(1 << 23)		
#define SDBCON_RBUFEN		(1 << 22)		
#define SDBCON_FLUSHWFIFO	(1 << 21)		
#define SDBCON_RBUFINV		(1 << 20)		
#define SDBCON_SDINI		(3 << 16)		
#define SDBCON_SDMODE		(0x3fff << 0)		

#define REFTIM_REFTIM		(0xffff << 0)		


#endif
