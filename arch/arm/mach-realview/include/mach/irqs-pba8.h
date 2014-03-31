/*
 * arch/arm/mach-realview/include/mach/irqs-pba8.h
 *
 * Copyright (C) 2008 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef __MACH_IRQS_PBA8_H
#define __MACH_IRQS_PBA8_H

#define IRQ_PBA8_GIC_START			32

#define IRQ_PBA8_WATCHDOG	(IRQ_PBA8_GIC_START + 0)	
#define IRQ_PBA8_SOFT		(IRQ_PBA8_GIC_START + 1)	
#define IRQ_PBA8_COMMRx		(IRQ_PBA8_GIC_START + 2)	
#define IRQ_PBA8_COMMTx		(IRQ_PBA8_GIC_START + 3)	
#define IRQ_PBA8_TIMER0_1	(IRQ_PBA8_GIC_START + 4)	
#define IRQ_PBA8_TIMER2_3	(IRQ_PBA8_GIC_START + 5)	
#define IRQ_PBA8_GPIO0		(IRQ_PBA8_GIC_START + 6)	
#define IRQ_PBA8_GPIO1		(IRQ_PBA8_GIC_START + 7)	
#define IRQ_PBA8_GPIO2		(IRQ_PBA8_GIC_START + 8)	
								
#define IRQ_PBA8_RTC		(IRQ_PBA8_GIC_START + 10)	
#define IRQ_PBA8_SSP		(IRQ_PBA8_GIC_START + 11)	
#define IRQ_PBA8_UART0		(IRQ_PBA8_GIC_START + 12)	
#define IRQ_PBA8_UART1		(IRQ_PBA8_GIC_START + 13)	
#define IRQ_PBA8_UART2		(IRQ_PBA8_GIC_START + 14)	
#define IRQ_PBA8_UART3		(IRQ_PBA8_GIC_START + 15)	
#define IRQ_PBA8_SCI		(IRQ_PBA8_GIC_START + 16)	
#define IRQ_PBA8_MMCI0A		(IRQ_PBA8_GIC_START + 17)	
#define IRQ_PBA8_MMCI0B		(IRQ_PBA8_GIC_START + 18)	
#define IRQ_PBA8_AACI		(IRQ_PBA8_GIC_START + 19)	
#define IRQ_PBA8_KMI0		(IRQ_PBA8_GIC_START + 20)	
#define IRQ_PBA8_KMI1		(IRQ_PBA8_GIC_START + 21)	
#define IRQ_PBA8_CHARLCD	(IRQ_PBA8_GIC_START + 22)	
#define IRQ_PBA8_CLCD		(IRQ_PBA8_GIC_START + 23)	
#define IRQ_PBA8_DMAC		(IRQ_PBA8_GIC_START + 24)	
#define IRQ_PBA8_PWRFAIL	(IRQ_PBA8_GIC_START + 25)	
#define IRQ_PBA8_PISMO		(IRQ_PBA8_GIC_START + 26)	
#define IRQ_PBA8_DoC		(IRQ_PBA8_GIC_START + 27)	
#define IRQ_PBA8_ETH		(IRQ_PBA8_GIC_START + 28)	
#define IRQ_PBA8_USB		(IRQ_PBA8_GIC_START + 29)	
#define IRQ_PBA8_TSPEN		(IRQ_PBA8_GIC_START + 30)	
#define IRQ_PBA8_TSKPAD		(IRQ_PBA8_GIC_START + 31)	

#define IRQ_PBA8_PMU		(IRQ_PBA8_GIC_START + 47)	

#define IRQ_PBA8_PCI0		(IRQ_PBA8_GIC_START + 50)
#define IRQ_PBA8_PCI1		(IRQ_PBA8_GIC_START + 51)
#define IRQ_PBA8_PCI2		(IRQ_PBA8_GIC_START + 52)
#define IRQ_PBA8_PCI3		(IRQ_PBA8_GIC_START + 53)

#define IRQ_PBA8_SMC		-1
#define IRQ_PBA8_SCTL		-1

#define NR_GIC_PBA8		1

#define NR_IRQS_PBA8		(IRQ_PBA8_GIC_START + 64)

#if defined(CONFIG_MACH_REALVIEW_PBA8)

#if !defined(NR_IRQS) || (NR_IRQS < NR_IRQS_PBA8)
#undef NR_IRQS
#define NR_IRQS			NR_IRQS_PBA8
#endif

#if !defined(MAX_GIC_NR) || (MAX_GIC_NR < NR_GIC_PBA8)
#undef MAX_GIC_NR
#define MAX_GIC_NR		NR_GIC_PBA8
#endif

#endif	

#endif	
