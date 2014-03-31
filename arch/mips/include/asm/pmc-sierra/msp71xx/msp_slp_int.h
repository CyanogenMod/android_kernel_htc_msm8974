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

#ifndef _MSP_SLP_INT_H
#define _MSP_SLP_INT_H


#define MSP_MIPS_INTBASE	0
#define MSP_INT_SW0		0  
#define MSP_INT_SW1		1  
#define MSP_INT_MAC0 		2  
#define MSP_INT_MAC1		3  
#define MSP_INT_C_IRQ2		4  
#define MSP_INT_VE		5  
#define MSP_INT_SLP		6  
#define MSP_INT_TIMER		7  

#define MSP_SLP_INTBASE		(MSP_MIPS_INTBASE + 8)
#define MSP_INT_EXT0		(MSP_SLP_INTBASE + 0)
					
#define MSP_INT_EXT1		(MSP_SLP_INTBASE + 1)
					
#define MSP_INT_EXT2		(MSP_SLP_INTBASE + 2)
					
#define MSP_INT_EXT3		(MSP_SLP_INTBASE + 3)
					

#define MSP_INT_SLP_VE		(MSP_SLP_INTBASE + 8)
					
#define MSP_INT_SLP_TDM		(MSP_SLP_INTBASE + 9)
					
#define MSP_INT_SLP_MAC0	(MSP_SLP_INTBASE + 10)
					
#define MSP_INT_SLP_MAC1	(MSP_SLP_INTBASE + 11)
					
#define MSP_INT_SEC		(MSP_SLP_INTBASE + 12)
					
#define	MSP_INT_PER		(MSP_SLP_INTBASE + 13)
					
#define	MSP_INT_TIMER0		(MSP_SLP_INTBASE + 14)
					
#define	MSP_INT_TIMER1		(MSP_SLP_INTBASE + 15)
					
#define	MSP_INT_TIMER2		(MSP_SLP_INTBASE + 16)
					
#define	MSP_INT_SLP_TIMER	(MSP_SLP_INTBASE + 17)
					
#define MSP_INT_BLKCP		(MSP_SLP_INTBASE + 18)
					
#define MSP_INT_UART0		(MSP_SLP_INTBASE + 19)
					
#define MSP_INT_PCI		(MSP_SLP_INTBASE + 20)
					
#define MSP_INT_PCI_DBELL	(MSP_SLP_INTBASE + 21)
					
#define MSP_INT_PCI_MSI		(MSP_SLP_INTBASE + 22)
					
#define MSP_INT_PCI_BC0		(MSP_SLP_INTBASE + 23)
					
#define MSP_INT_PCI_BC1		(MSP_SLP_INTBASE + 24)
					
#define MSP_INT_SLP_ERR		(MSP_SLP_INTBASE + 25)
					
#define MSP_INT_MAC2		(MSP_SLP_INTBASE + 26)
					

#define MSP_PER_INTBASE		(MSP_SLP_INTBASE + 32)
#define MSP_INT_UART1		(MSP_PER_INTBASE + 2)
					
#define MSP_INT_2WIRE		(MSP_PER_INTBASE + 6)
					
#define MSP_INT_TM0		(MSP_PER_INTBASE + 7)
					
#define MSP_INT_TM1		(MSP_PER_INTBASE + 8)
					
#define MSP_INT_SPRX		(MSP_PER_INTBASE + 10)
					
#define MSP_INT_SPTX		(MSP_PER_INTBASE + 11)
					
#define MSP_INT_GPIO		(MSP_PER_INTBASE + 12)
					
#define MSP_INT_PER_ERR		(MSP_PER_INTBASE + 13)
					

#endif 
