/*
 * Copyright 2005-2008 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later
 */

#ifndef _BF533_IRQ_H_
#define _BF533_IRQ_H_

#include <mach-common/irq.h>

#define NR_PERI_INTS		24

#define IRQ_PLL_WAKEUP		BFIN_IRQ(0)	
#define IRQ_DMA_ERROR		BFIN_IRQ(1)	
#define IRQ_PPI_ERROR		BFIN_IRQ(2)	
#define IRQ_SPORT0_ERROR	BFIN_IRQ(3)	
#define IRQ_SPORT1_ERROR	BFIN_IRQ(4)	
#define IRQ_SPI_ERROR		BFIN_IRQ(5)	
#define IRQ_UART0_ERROR		BFIN_IRQ(6)	
#define IRQ_RTC			BFIN_IRQ(7)	
#define IRQ_PPI			BFIN_IRQ(8)	
#define IRQ_SPORT0_RX		BFIN_IRQ(9)	
#define IRQ_SPORT0_TX		BFIN_IRQ(10)	
#define IRQ_SPORT1_RX		BFIN_IRQ(11)	
#define IRQ_SPORT1_TX		BFIN_IRQ(12)	
#define IRQ_SPI			BFIN_IRQ(13)	
#define IRQ_UART0_RX		BFIN_IRQ(14)	
#define IRQ_UART0_TX		BFIN_IRQ(15)	
#define IRQ_TIMER0		BFIN_IRQ(16)	
#define IRQ_TIMER1		BFIN_IRQ(17)	
#define IRQ_TIMER2		BFIN_IRQ(18)	
#define IRQ_PROG_INTA		BFIN_IRQ(19)	
#define IRQ_PROG_INTB		BFIN_IRQ(20)	
#define IRQ_MEM_DMA0		BFIN_IRQ(21)	
#define IRQ_MEM_DMA1		BFIN_IRQ(22)	
#define IRQ_WATCH		BFIN_IRQ(23)	

#define SYS_IRQS		31

#define IRQ_PF0			33
#define IRQ_PF1			34
#define IRQ_PF2			35
#define IRQ_PF3			36
#define IRQ_PF4			37
#define IRQ_PF5			38
#define IRQ_PF6			39
#define IRQ_PF7			40
#define IRQ_PF8			41
#define IRQ_PF9			42
#define IRQ_PF10		43
#define IRQ_PF11		44
#define IRQ_PF12		45
#define IRQ_PF13		46
#define IRQ_PF14		47
#define IRQ_PF15		48

#define GPIO_IRQ_BASE		IRQ_PF0

#define NR_MACH_IRQS		(IRQ_PF15 + 1)

#define RTC_ERROR_POS		28
#define UART_ERROR_POS		24
#define SPORT1_ERROR_POS	20
#define SPI_ERROR_POS		16
#define SPORT0_ERROR_POS	12
#define PPI_ERROR_POS		8
#define DMA_ERROR_POS		4
#define PLLWAKE_ERROR_POS	0

#define DMA7_UARTTX_POS		28
#define DMA6_UARTRX_POS		24
#define DMA5_SPI_POS		20
#define DMA4_SPORT1TX_POS	16
#define DMA3_SPORT1RX_POS	12
#define DMA2_SPORT0TX_POS	8
#define DMA1_SPORT0RX_POS	4
#define DMA0_PPI_POS		0

#define WDTIMER_POS		28
#define MEMDMA1_POS		24
#define MEMDMA0_POS		20
#define PFB_POS			16
#define PFA_POS			12
#define TIMER2_POS		8
#define TIMER1_POS		4
#define TIMER0_POS		0

#endif
