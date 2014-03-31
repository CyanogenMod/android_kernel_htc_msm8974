/* MN10300 On-board I/O port module registers
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _ASM_PIO_REGS_H
#define _ASM_PIO_REGS_H

#include <asm/cpu-regs.h>
#include <asm/intctl-regs.h>

#ifdef __KERNEL__

#define	P0MD			__SYSREG(0xdb000000, u16)	
#define P0MD_0			0x0003	
#define P0MD_0_IN		0x0000	
#define P0MD_0_OUT		0x0001	
#define P0MD_0_TM0IO		0x0002	
#define P0MD_0_EYECLK		0x0003	
#define P0MD_1			0x000c
#define P0MD_1_IN		0x0000
#define P0MD_1_OUT		0x0004
#define P0MD_1_TM1IO		0x0008	
#define P0MD_1_EYED		0x000c	
#define P0MD_2			0x0030
#define P0MD_2_IN		0x0000
#define P0MD_2_OUT		0x0010
#define P0MD_2_TM2IO		0x0020	
#define P0MD_3			0x00c0
#define P0MD_3_IN		0x0000
#define P0MD_3_OUT		0x0040
#define P0MD_3_TM3IO		0x0080	
#define P0MD_4			0x0300
#define P0MD_4_IN		0x0000
#define P0MD_4_OUT		0x0100
#define P0MD_4_TM4IO		0x0200	
#define P0MD_4_XCTS		0x0300	
#define P0MD_5			0x0c00
#define P0MD_5_IN		0x0000
#define P0MD_5_OUT		0x0400
#define P0MD_5_TM5IO		0x0800	
#define P0MD_6			0x3000
#define P0MD_6_IN		0x0000
#define P0MD_6_OUT		0x1000
#define P0MD_6_TM6IOA		0x2000	
#define P0MD_7			0xc000
#define P0MD_7_IN		0x0000
#define P0MD_7_OUT		0x4000
#define P0MD_7_TM6IOB		0x8000	

#define	P0IN			__SYSREG(0xdb000004, u8)	
#define	P0OUT			__SYSREG(0xdb000008, u8)	

#define	P0TMIO			__SYSREG(0xdb00000c, u8)	
#define P0TMIO_TM0_IN		0x00
#define P0TMIO_TM0_OUT		0x01
#define P0TMIO_TM1_IN		0x00
#define P0TMIO_TM1_OUT		0x02
#define P0TMIO_TM2_IN		0x00
#define P0TMIO_TM2_OUT		0x04
#define P0TMIO_TM3_IN		0x00
#define P0TMIO_TM3_OUT		0x08
#define P0TMIO_TM4_IN		0x00
#define P0TMIO_TM4_OUT		0x10
#define P0TMIO_TM5_IN		0x00
#define P0TMIO_TM5_OUT		0x20
#define P0TMIO_TM6A_IN		0x00
#define P0TMIO_TM6A_OUT		0x40
#define P0TMIO_TM6B_IN		0x00
#define P0TMIO_TM6B_OUT		0x80

#define	P1MD			__SYSREG(0xdb000100, u16)	
#define P1MD_0			0x0003	
#define P1MD_0_IN		0x0000	
#define P1MD_0_OUT		0x0001	
#define P1MD_0_TM7IO		0x0002	
#define P1MD_0_ADTRG		0x0003	
#define P1MD_1			0x000c
#define P1MD_1_IN		0x0000
#define P1MD_1_OUT		0x0004
#define P1MD_1_TM8IO		0x0008	
#define P1MD_1_XDMR0		0x000c	
#define P1MD_2			0x0030
#define P1MD_2_IN		0x0000
#define P1MD_2_OUT		0x0010
#define P1MD_2_TM9IO		0x0020	
#define P1MD_2_XDMR1		0x0030	
#define P1MD_3			0x00c0
#define P1MD_3_IN		0x0000
#define P1MD_3_OUT		0x0040
#define P1MD_3_TM10IO		0x0080	
#define P1MD_3_FRQS0		0x00c0	
#define P1MD_4			0x0300
#define P1MD_4_IN		0x0000
#define P1MD_4_OUT		0x0100
#define P1MD_4_TM11IO		0x0200	
#define P1MD_4_FRQS1		0x0300	

#define	P1IN			__SYSREG(0xdb000104, u8)	
#define	P1OUT			__SYSREG(0xdb000108, u8)	
#define	P1TMIO			__SYSREG(0xdb00010c, u8)	
#define P1TMIO_TM11_IN		0x00
#define P1TMIO_TM11_OUT		0x01
#define P1TMIO_TM10_IN		0x00
#define P1TMIO_TM10_OUT		0x02
#define P1TMIO_TM9_IN		0x00
#define P1TMIO_TM9_OUT		0x04
#define P1TMIO_TM8_IN		0x00
#define P1TMIO_TM8_OUT		0x08
#define P1TMIO_TM7_IN		0x00
#define P1TMIO_TM7_OUT		0x10

#define	P2MD			__SYSREG(0xdb000200, u16)	
#define P2MD_0			0x0003	
#define P2MD_0_IN		0x0000	
#define P2MD_0_OUT		0x0001	
#define P2MD_0_BOOTBW		0x0003	
#define P2MD_1			0x000c
#define P2MD_1_IN		0x0000
#define P2MD_1_OUT		0x0004
#define P2MD_1_BOOTSEL		0x000c	
#define P2MD_2			0x0030
#define P2MD_2_IN		0x0000
#define P2MD_2_OUT		0x0010
#define P2MD_3			0x00c0
#define P2MD_3_IN		0x0000
#define P2MD_3_OUT		0x0040
#define P2MD_3_CKIO		0x00c0	
#define P2MD_4			0x0300
#define P2MD_4_IN		0x0000
#define P2MD_4_OUT		0x0100
#define P2MD_4_CMOD		0x0300	

#define	P2IN			__SYSREG(0xdb000204, u8)	
#define	P2OUT			__SYSREG(0xdb000208, u8)	
#define	P2TMIO			__SYSREG(0xdb00020c, u8)	

#define	P3MD			__SYSREG(0xdb000300, u16)	
#define P3MD_0			0x0003	
#define P3MD_0_IN		0x0000	
#define P3MD_0_OUT		0x0001	
#define P3MD_0_AFRXD		0x0002	
#define P3MD_1			0x000c
#define P3MD_1_IN		0x0000
#define P3MD_1_OUT		0x0004
#define P3MD_1_AFTXD		0x0008	
#define P3MD_2			0x0030
#define P3MD_2_IN		0x0000
#define P3MD_2_OUT		0x0010
#define P3MD_2_AFSCLK		0x0020	
#define P3MD_3			0x00c0
#define P3MD_3_IN		0x0000
#define P3MD_3_OUT		0x0040
#define P3MD_3_AFFS		0x0080	
#define P3MD_4			0x0300
#define P3MD_4_IN		0x0000
#define P3MD_4_OUT		0x0100
#define P3MD_4_AFEHC		0x0200	

#define	P3IN			__SYSREG(0xdb000304, u8)	
#define	P3OUT			__SYSREG(0xdb000308, u8)	

#define	P4MD			__SYSREG(0xdb000400, u16)	
#define P4MD_0			0x0003	
#define P4MD_0_IN		0x0000	
#define P4MD_0_OUT		0x0001	
#define P4MD_0_SCL0		0x0002	
#define P4MD_1			0x000c
#define P4MD_1_IN		0x0000
#define P4MD_1_OUT		0x0004
#define P4MD_1_SDA0		0x0008
#define P4MD_2			0x0030
#define P4MD_2_IN		0x0000
#define P4MD_2_OUT		0x0010
#define P4MD_2_SCL1		0x0020
#define P4MD_3			0x00c0
#define P4MD_3_IN		0x0000
#define P4MD_3_OUT		0x0040
#define P4MD_3_SDA1		0x0080
#define P4MD_4			0x0300
#define P4MD_4_IN		0x0000
#define P4MD_4_OUT		0x0100
#define P4MD_4_SBO0		0x0200
#define P4MD_5			0x0c00
#define P4MD_5_IN		0x0000
#define P4MD_5_OUT		0x0400
#define P4MD_5_SBO1		0x0800
#define P4MD_6			0x3000
#define P4MD_6_IN		0x0000
#define P4MD_6_OUT		0x1000
#define P4MD_6_SBT0		0x2000
#define P4MD_7			0xc000
#define P4MD_7_IN		0x0000
#define P4MD_7_OUT		0x4000
#define P4MD_7_SBT1		0x8000

#define	P4IN			__SYSREG(0xdb000404, u8)	
#define	P4OUT			__SYSREG(0xdb000408, u8)	

#define	P5MD			__SYSREG(0xdb000500, u16)	
#define P5MD_0			0x0003	
#define P5MD_0_IN		0x0000	
#define P5MD_0_OUT		0x0001	
#define P5MD_0_IRTXD		0x0002	
#define P5MD_0_SOUT		0x0004	
#define P5MD_1			0x000c
#define P5MD_1_IN		0x0000
#define P5MD_1_OUT		0x0004
#define P5MD_1_IRRXDS		0x0008	
#define P5MD_1_SIN		0x000c	
#define P5MD_2			0x0030
#define P5MD_2_IN		0x0000
#define P5MD_2_OUT		0x0010
#define P5MD_2_IRRXDF		0x0020	

#define	P5IN			__SYSREG(0xdb000504, u8)	
#define	P5OUT			__SYSREG(0xdb000508, u8)	


#endif 

#endif 
