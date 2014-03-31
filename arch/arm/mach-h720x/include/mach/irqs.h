/*
 * arch/arm/mach-h720x/include/mach/irqs.h
 *
 * Copyright (C) 2000 Jungjun Kim
 *           (C) 2003 Robert Schwebel <r.schwebel@pengutronix.de>
 *           (C) 2003 Thomas Gleixner <tglx@linutronix.de>
 *
 */

#ifndef __ASM_ARCH_IRQS_H
#define __ASM_ARCH_IRQS_H

#if defined (CONFIG_CPU_H7201)

#define IRQ_PMU		0		
#define IRQ_DMA		1 		
#define IRQ_LCD		2		
#define IRQ_VGA		3 		
#define IRQ_PCMCIA1 	4 		
#define IRQ_PCMCIA2 	5 		
#define IRQ_AFE		6 		
#define IRQ_AIC		7 		
#define IRQ_KEYBOARD 	8 		
#define IRQ_TIMER0	9 		
#define IRQ_RTC		10		
#define IRQ_SOUND	11		
#define IRQ_USB		12		
#define IRQ_IrDA 	13		
#define IRQ_UART0	14		
#define IRQ_UART1	15		
#define IRQ_SPI		16		
#define IRQ_GPIOA 	17		
#define IRQ_GPIOB	18		
#define IRQ_GPIOC	19		
#define IRQ_GPIOD	20		
#define IRQ_CommRX	21		
#define IRQ_CommTX	22		
#define IRQ_Soft	23		

#define NR_GLBL_IRQS	24

#define IRQ_CHAINED_GPIOA(x)  (NR_GLBL_IRQS + x)
#define IRQ_CHAINED_GPIOB(x)  (IRQ_CHAINED_GPIOA(32) + x)
#define IRQ_CHAINED_GPIOC(x)  (IRQ_CHAINED_GPIOB(32) + x)
#define IRQ_CHAINED_GPIOD(x)  (IRQ_CHAINED_GPIOC(32) + x)
#define NR_IRQS               IRQ_CHAINED_GPIOD(32)

#define IRQ_ENA_MUX	(1<<IRQ_GPIOA) | (1<<IRQ_GPIOB) \
			| (1<<IRQ_GPIOC) | (1<<IRQ_GPIOD)


#elif defined (CONFIG_CPU_H7202)

#define IRQ_PMU		0		
#define IRQ_DMA		1		
#define IRQ_LCD		2		
#define IRQ_SOUND	3		
#define IRQ_I2S		4		
#define IRQ_USB 	5		
#define IRQ_MMC 	6		
#define IRQ_RTC 	7		
#define IRQ_UART0 	8		
#define IRQ_UART1 	9		
#define IRQ_UART2 	10		
#define IRQ_UART3 	11		
#define IRQ_KBD 	12		
#define IRQ_PS2 	13		
#define IRQ_AIC 	14		
#define IRQ_TIMER0 	15		
#define IRQ_TIMERX 	16		
#define IRQ_WDT 	17		
#define IRQ_CAN0 	18		
#define IRQ_CAN1 	19		
#define IRQ_EXT0 	20		
#define IRQ_EXT1 	21		
#define IRQ_GPIOA 	22		
#define IRQ_GPIOB 	23		
#define IRQ_GPIOC 	24		
#define IRQ_GPIOD 	25		
#define IRQ_GPIOE 	26		
#define IRQ_COMMRX 	27		
#define IRQ_COMMTX 	28		
#define IRQ_SMC 	29		
#define IRQ_Soft 	30		
#define IRQ_RESERVED1 	31		
#define NR_GLBL_IRQS	32

#define NR_TIMERX_IRQS	3

#define IRQ_CHAINED_GPIOA(x)  (NR_GLBL_IRQS + x)
#define IRQ_CHAINED_GPIOB(x)  (IRQ_CHAINED_GPIOA(32) + x)
#define IRQ_CHAINED_GPIOC(x)  (IRQ_CHAINED_GPIOB(32) + x)
#define IRQ_CHAINED_GPIOD(x)  (IRQ_CHAINED_GPIOC(32) + x)
#define IRQ_CHAINED_GPIOE(x)  (IRQ_CHAINED_GPIOD(32) + x)
#define IRQ_CHAINED_TIMERX(x) (IRQ_CHAINED_GPIOE(32) + x)
#define IRQ_TIMER1            (IRQ_CHAINED_TIMERX(0))
#define IRQ_TIMER2            (IRQ_CHAINED_TIMERX(1))
#define IRQ_TIMER64B          (IRQ_CHAINED_TIMERX(2))

#define NR_IRQS		(IRQ_CHAINED_TIMERX(NR_TIMERX_IRQS))

#define IRQ_ENA_MUX	(1<<IRQ_TIMERX) | (1<<IRQ_GPIOA) | (1<<IRQ_GPIOB) | \
			(1<<IRQ_GPIOC) 	| (1<<IRQ_GPIOD) | (1<<IRQ_GPIOE) | \
			(1<<IRQ_TIMERX)

#else
#error cpu definition mismatch
#endif

#define IRQ_TO_REGNO(irq) ((irq - NR_GLBL_IRQS) >> 5)
#define IRQ_TO_BIT(irq) (1 << ((irq - NR_GLBL_IRQS) % 32))

#endif
