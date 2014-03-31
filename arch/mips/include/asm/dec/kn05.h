/*
 *	include/asm-mips/dec/kn05.h
 *
 *	DECstation/DECsystem 5000/260 (4max+ or KN05), 5000/150 (4min
 *	or KN04-BA), Personal DECstation/DECsystem 5000/50 (4maxine or
 *	KN04-CA) and DECsystem 5900/260 (KN05) R4k CPU card MB ASIC
 *	definitions.
 *
 *	Copyright (C) 2002, 2003, 2005, 2008  Maciej W. Rozycki
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	WARNING!  All this information is pure guesswork based on the
 *	ROM.  It is provided here in hope it will give someone some
 *	food for thought.  No documentation for the KN05 nor the KN04
 *	module has been located so far.
 */
#ifndef __ASM_MIPS_DEC_KN05_H
#define __ASM_MIPS_DEC_KN05_H

#include <asm/dec/ioasic_addrs.h>

#define KN4K_SLOT_BASE	0x1fc00000

#define KN4K_MB_ROM	(0*IOASIC_SLOT_SIZE)	
#define KN4K_IOCTL	(1*IOASIC_SLOT_SIZE)	
#define KN4K_ESAR	(2*IOASIC_SLOT_SIZE)	
#define KN4K_LANCE	(3*IOASIC_SLOT_SIZE)	
#define KN4K_MB_INT	(4*IOASIC_SLOT_SIZE)	
#define KN4K_MB_EA	(5*IOASIC_SLOT_SIZE)	
#define KN4K_MB_EC	(6*IOASIC_SLOT_SIZE)	
#define KN4K_MB_CSR	(7*IOASIC_SLOT_SIZE)	
#define KN4K_RES_08	(8*IOASIC_SLOT_SIZE)	
#define KN4K_RES_09	(9*IOASIC_SLOT_SIZE)	
#define KN4K_RES_10	(10*IOASIC_SLOT_SIZE)	
#define KN4K_RES_11	(11*IOASIC_SLOT_SIZE)	
#define KN4K_SCSI	(12*IOASIC_SLOT_SIZE)	
#define KN4K_RES_13	(13*IOASIC_SLOT_SIZE)	
#define KN4K_RES_14	(14*IOASIC_SLOT_SIZE)	
#define KN4K_RES_15	(15*IOASIC_SLOT_SIZE)	

#define KN4K_MB_INT_TC		(1<<0)		
#define KN4K_MB_INT_RTC		(1<<1)		
#define KN4K_MB_INT_MT		(1<<3)		

#define KN4K_MB_CSR_PF		(1<<0)		
#define KN4K_MB_CSR_F		(1<<1)		
#define KN4K_MB_CSR_ECC		(0xff<<2)	
#define KN4K_MB_CSR_OD		(1<<10)		
#define KN4K_MB_CSR_CP		(1<<11)		
#define KN4K_MB_CSR_UNC		(1<<12)		
#define KN4K_MB_CSR_IM		(1<<13)		
#define KN4K_MB_CSR_NC		(1<<14)		
#define KN4K_MB_CSR_EE		(1<<15)		
#define KN4K_MB_CSR_MSK		(0x1f<<16)	
#define KN4K_MB_CSR_FW		(1<<21)		
#define KN4K_MB_CSR_W		(1<<31)		

#endif 
