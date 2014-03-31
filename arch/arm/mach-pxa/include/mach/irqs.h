/*
 *  arch/arm/mach-pxa/include/mach/irqs.h
 *
 *  Author:	Nicolas Pitre
 *  Created:	Jun 15, 2001
 *  Copyright:	MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_MACH_IRQS_H
#define __ASM_MACH_IRQS_H

#ifdef CONFIG_PXA_HAVE_ISA_IRQS
#define PXA_ISA_IRQ(x)	(x)
#define PXA_ISA_IRQ_NUM	(16)
#else
#define PXA_ISA_IRQ_NUM	(0)
#endif

#define PXA_IRQ(x)	(PXA_ISA_IRQ_NUM + (x))

#define IRQ_SSP3	PXA_IRQ(0)	
#define IRQ_MSL		PXA_IRQ(1)	
#define IRQ_USBH2	PXA_IRQ(2)	
#define IRQ_USBH1	PXA_IRQ(3)	
#define IRQ_KEYPAD	PXA_IRQ(4)	
#define IRQ_MEMSTK	PXA_IRQ(5)	
#define IRQ_ACIPC0	PXA_IRQ(5)	
#define IRQ_PWRI2C	PXA_IRQ(6)	
#define IRQ_HWUART	PXA_IRQ(7)	
#define IRQ_OST_4_11	PXA_IRQ(7)	
#define	IRQ_GPIO0	PXA_IRQ(8)	
#define	IRQ_GPIO1	PXA_IRQ(9)	
#define	IRQ_GPIO_2_x	PXA_IRQ(10)	
#define	IRQ_USB		PXA_IRQ(11)	
#define	IRQ_PMU		PXA_IRQ(12)	
#define	IRQ_I2S		PXA_IRQ(13)	
#define IRQ_SSP4	PXA_IRQ(13)	
#define	IRQ_AC97	PXA_IRQ(14)	
#define IRQ_ASSP	PXA_IRQ(15)	
#define IRQ_USIM	PXA_IRQ(15)     
#define IRQ_NSSP	PXA_IRQ(16)	
#define IRQ_SSP2	PXA_IRQ(16)	
#define	IRQ_LCD		PXA_IRQ(17)	
#define	IRQ_I2C		PXA_IRQ(18)	
#define	IRQ_ICP		PXA_IRQ(19)	
#define IRQ_ACIPC2	PXA_IRQ(19)	
#define	IRQ_STUART	PXA_IRQ(20)	
#define	IRQ_BTUART	PXA_IRQ(21)	
#define	IRQ_FFUART	PXA_IRQ(22)	
#define	IRQ_MMC		PXA_IRQ(23)	
#define	IRQ_SSP		PXA_IRQ(24)	
#define	IRQ_DMA 	PXA_IRQ(25)	
#define	IRQ_OST0 	PXA_IRQ(26)	
#define	IRQ_OST1 	PXA_IRQ(27)	
#define	IRQ_OST2 	PXA_IRQ(28)	
#define	IRQ_OST3 	PXA_IRQ(29)	
#define	IRQ_RTC1Hz	PXA_IRQ(30)	
#define	IRQ_RTCAlrm	PXA_IRQ(31)	

#define IRQ_TPM		PXA_IRQ(32)	
#define IRQ_CAMERA	PXA_IRQ(33)	
#define IRQ_CIR		PXA_IRQ(34)	
#define IRQ_COMM_WDT	PXA_IRQ(35) 	
#define IRQ_TSI		PXA_IRQ(36)	
#define IRQ_ENHROT	PXA_IRQ(37)	
#define IRQ_USIM2	PXA_IRQ(38)	
#define IRQ_GCU		PXA_IRQ(39)	
#define IRQ_ACIPC1	PXA_IRQ(40)	
#define IRQ_MMC2	PXA_IRQ(41)	
#define IRQ_TRKBALL	PXA_IRQ(43)	
#define IRQ_1WIRE	PXA_IRQ(44)	
#define IRQ_NAND	PXA_IRQ(45)	
#define IRQ_USB2	PXA_IRQ(46)	
#define IRQ_WAKEUP0	PXA_IRQ(49)	
#define IRQ_WAKEUP1	PXA_IRQ(50)	
#define IRQ_DMEMC	PXA_IRQ(51)	
#define IRQ_MMC3	PXA_IRQ(55)	

#define IRQ_U2O		PXA_IRQ(64)	
#define IRQ_U2H		PXA_IRQ(65)	
#define IRQ_PXA935_MMC0	PXA_IRQ(72)	
#define IRQ_PXA935_MMC1	PXA_IRQ(73)	
#define IRQ_PXA935_MMC2	PXA_IRQ(74)	
#define IRQ_PXA955_MMC3	PXA_IRQ(75)	
#define IRQ_U2P		PXA_IRQ(93)	

#define PXA_GPIO_IRQ_BASE	PXA_IRQ(96)
#define PXA_NR_BUILTIN_GPIO	(192)
#define PXA_GPIO_TO_IRQ(x)	(PXA_GPIO_IRQ_BASE + (x))

#define IRQ_BOARD_START		(PXA_GPIO_IRQ_BASE + PXA_NR_BUILTIN_GPIO)

#define PXA_NR_IRQS		(IRQ_BOARD_START)

#ifndef __ASSEMBLY__
struct irq_data;
struct pt_regs;

void pxa_mask_irq(struct irq_data *);
void pxa_unmask_irq(struct irq_data *);
void icip_handle_irq(struct pt_regs *);
void ichp_handle_irq(struct pt_regs *);

void pxa_init_irq(int irq_nr, int (*set_wake)(struct irq_data *, unsigned int));
#endif

#endif 
