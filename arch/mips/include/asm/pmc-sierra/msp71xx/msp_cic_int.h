/*
 * Defines for the MSP interrupt controller.
 *
 * Copyright (C) 1999 MIPS Technologies, Inc.  All rights reserved.
 * Author: Carsten Langgaard, carstenl@mips.com
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 */

#ifndef _MSP_CIC_INT_H
#define _MSP_CIC_INT_H



#define MSP_MIPS_INTBASE	0
#define MSP_INT_SW0		0	
#define MSP_INT_SW1		1	
#define MSP_INT_MAC0		2	
#define MSP_INT_MAC1		3	
#define MSP_INT_USB		4	
#define MSP_INT_SAR		5	
#define MSP_INT_CIC		6	
#define MSP_INT_SEC		7	

#define MSP_CIC_INTBASE		(MSP_MIPS_INTBASE + 8)
#define MSP_INT_EXT0		(MSP_CIC_INTBASE + 0)
					
#define MSP_INT_EXT1		(MSP_CIC_INTBASE + 1)
					
#define MSP_INT_EXT2		(MSP_CIC_INTBASE + 2)
					
#define MSP_INT_EXT3		(MSP_CIC_INTBASE + 3)
					
#define MSP_INT_CPUIF		(MSP_CIC_INTBASE + 4)
					
#define MSP_INT_EXT4		(MSP_CIC_INTBASE + 5)
					
#define MSP_INT_CIC_USB		(MSP_CIC_INTBASE + 6)
					
#define MSP_INT_MBOX		(MSP_CIC_INTBASE + 7)
					
#define MSP_INT_EXT5		(MSP_CIC_INTBASE + 8)
					
#define MSP_INT_TDM		(MSP_CIC_INTBASE + 9)
					
#define MSP_INT_CIC_MAC0	(MSP_CIC_INTBASE + 10)
					
#define MSP_INT_CIC_MAC1	(MSP_CIC_INTBASE + 11)
					
#define MSP_INT_CIC_SEC		(MSP_CIC_INTBASE + 12)
					
#define	MSP_INT_PER		(MSP_CIC_INTBASE + 13)
					
#define	MSP_INT_TIMER0		(MSP_CIC_INTBASE + 14)
					
#define	MSP_INT_TIMER1		(MSP_CIC_INTBASE + 15)
					
#define	MSP_INT_TIMER2		(MSP_CIC_INTBASE + 16)
					
#define	MSP_INT_VPE0_TIMER	(MSP_CIC_INTBASE + 17)
					
#define MSP_INT_BLKCP		(MSP_CIC_INTBASE + 18)
					
#define MSP_INT_UART0		(MSP_CIC_INTBASE + 19)
					
#define MSP_INT_PCI		(MSP_CIC_INTBASE + 20)
					
#define MSP_INT_EXT6		(MSP_CIC_INTBASE + 21)
					
#define MSP_INT_PCI_MSI		(MSP_CIC_INTBASE + 22)
					
#define MSP_INT_CIC_SAR		(MSP_CIC_INTBASE + 23)
					
#define MSP_INT_DSL		(MSP_CIC_INTBASE + 24)
					
#define MSP_INT_CIC_ERR		(MSP_CIC_INTBASE + 25)
					
#define MSP_INT_VPE1_TIMER	(MSP_CIC_INTBASE + 26)
					
#define MSP_INT_VPE0_PC		(MSP_CIC_INTBASE + 27)
					
#define MSP_INT_VPE1_PC		(MSP_CIC_INTBASE + 28)
					
#define MSP_INT_EXT7		(MSP_CIC_INTBASE + 29)
					
#define MSP_INT_VPE0_SW		(MSP_CIC_INTBASE + 30)
					
#define MSP_INT_VPE1_SW		(MSP_CIC_INTBASE + 31)
					

#define MSP_PER_INTBASE		(MSP_CIC_INTBASE + 32)
#define MSP_INT_UART1		(MSP_PER_INTBASE + 2)
					
#define MSP_INT_2WIRE		(MSP_PER_INTBASE + 6)
					
#define MSP_INT_TM0		(MSP_PER_INTBASE + 7)
					
#define MSP_INT_TM1		(MSP_PER_INTBASE + 8)
					
#define MSP_INT_SPRX		(MSP_PER_INTBASE + 10)
					
#define MSP_INT_SPTX		(MSP_PER_INTBASE + 11)
					
#define MSP_INT_GPIO		(MSP_PER_INTBASE + 12)
					
#define MSP_INT_PER_ERR		(MSP_PER_INTBASE + 13)
					

#endif 
