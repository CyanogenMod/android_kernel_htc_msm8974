/*
 * Copyright 2005-2008 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later
 */

#ifndef _BF537_IRQ_H_
#define _BF537_IRQ_H_

#include <mach-common/irq.h>

#define NR_PERI_INTS		32

#define IRQ_PLL_WAKEUP		BFIN_IRQ(0)	
#define IRQ_DMA_ERROR		BFIN_IRQ(1)	
#define IRQ_GENERIC_ERROR	BFIN_IRQ(2)	
#define IRQ_RTC			BFIN_IRQ(3)	
#define IRQ_PPI			BFIN_IRQ(4)	
#define IRQ_SPORT0_RX		BFIN_IRQ(5)	
#define IRQ_SPORT0_TX		BFIN_IRQ(6)	
#define IRQ_SPORT1_RX		BFIN_IRQ(7)	
#define IRQ_SPORT1_TX		BFIN_IRQ(8)	
#define IRQ_TWI			BFIN_IRQ(9)	
#define IRQ_SPI			BFIN_IRQ(10)	
#define IRQ_UART0_RX		BFIN_IRQ(11)	
#define IRQ_UART0_TX		BFIN_IRQ(12)	
#define IRQ_UART1_RX		BFIN_IRQ(13)	
#define IRQ_UART1_TX		BFIN_IRQ(14)	
#define IRQ_CAN_RX		BFIN_IRQ(15)	
#define IRQ_CAN_TX		BFIN_IRQ(16)	
#define IRQ_PH_INTA_MAC_RX	BFIN_IRQ(17)	
#define IRQ_PH_INTB_MAC_TX	BFIN_IRQ(18)	
#define IRQ_TIMER0		BFIN_IRQ(19)	
#define IRQ_TIMER1		BFIN_IRQ(20)	
#define IRQ_TIMER2		BFIN_IRQ(21)	
#define IRQ_TIMER3		BFIN_IRQ(22)	
#define IRQ_TIMER4		BFIN_IRQ(23)	
#define IRQ_TIMER5		BFIN_IRQ(24)	
#define IRQ_TIMER6		BFIN_IRQ(25)	
#define IRQ_TIMER7		BFIN_IRQ(26)	
#define IRQ_PF_INTA_PG_INTA	BFIN_IRQ(27)	
#define IRQ_PORTG_INTB		BFIN_IRQ(28)	
#define IRQ_MEM_DMA0		BFIN_IRQ(29)	
#define IRQ_MEM_DMA1		BFIN_IRQ(30)	
#define IRQ_PF_INTB_WATCH	BFIN_IRQ(31)	

#define SYS_IRQS		39

#define IRQ_PPI_ERROR		42	
#define IRQ_CAN_ERROR		43	
#define IRQ_MAC_ERROR		44	
#define IRQ_SPORT0_ERROR	45	
#define IRQ_SPORT1_ERROR	46	
#define IRQ_SPI_ERROR		47	
#define IRQ_UART0_ERROR		48	
#define IRQ_UART1_ERROR		49	

#define IRQ_PF0			50
#define IRQ_PF1			51
#define IRQ_PF2			52
#define IRQ_PF3			53
#define IRQ_PF4			54
#define IRQ_PF5			55
#define IRQ_PF6			56
#define IRQ_PF7			57
#define IRQ_PF8			58
#define IRQ_PF9			59
#define IRQ_PF10		60
#define IRQ_PF11		61
#define IRQ_PF12		62
#define IRQ_PF13		63
#define IRQ_PF14		64
#define IRQ_PF15		65

#define IRQ_PG0			66
#define IRQ_PG1			67
#define IRQ_PG2			68
#define IRQ_PG3			69
#define IRQ_PG4			70
#define IRQ_PG5			71
#define IRQ_PG6			72
#define IRQ_PG7			73
#define IRQ_PG8			74
#define IRQ_PG9			75
#define IRQ_PG10		76
#define IRQ_PG11		77
#define IRQ_PG12		78
#define IRQ_PG13		79
#define IRQ_PG14		80
#define IRQ_PG15		81

#define IRQ_PH0			82
#define IRQ_PH1			83
#define IRQ_PH2			84
#define IRQ_PH3			85
#define IRQ_PH4			86
#define IRQ_PH5			87
#define IRQ_PH6			88
#define IRQ_PH7			89
#define IRQ_PH8			90
#define IRQ_PH9			91
#define IRQ_PH10		92
#define IRQ_PH11		93
#define IRQ_PH12		94
#define IRQ_PH13		95
#define IRQ_PH14		96
#define IRQ_PH15		97

#define GPIO_IRQ_BASE		IRQ_PF0

#define IRQ_MAC_PHYINT		98	
#define IRQ_MAC_MMCINT		99	
#define IRQ_MAC_RXFSINT		100	
#define IRQ_MAC_TXFSINT		101	
#define IRQ_MAC_WAKEDET		102	
#define IRQ_MAC_RXDMAERR	103	
#define IRQ_MAC_TXDMAERR	104	
#define IRQ_MAC_STMDONE		105	

#define IRQ_MAC_RX		106	
#define IRQ_PORTH_INTA		107	

#if 0 
#define IRQ_MAC_TX		108	
#define IRQ_PORTH_INTB		109	
#else
#define IRQ_MAC_TX		IRQ_PH_INTB_MAC_TX
#endif

#define IRQ_PORTF_INTA		110	
#define IRQ_PORTG_INTA		111	

#if 0 
#define IRQ_WATCH		112	
#define IRQ_PORTF_INTB		113	
#else
#define IRQ_WATCH		IRQ_PF_INTB_WATCH
#endif

#define NR_MACH_IRQS		(113 + 1)

#define IRQ_PLL_WAKEUP_POS	0
#define IRQ_DMA_ERROR_POS	4
#define IRQ_ERROR_POS		8
#define IRQ_RTC_POS		12
#define IRQ_PPI_POS		16
#define IRQ_SPORT0_RX_POS	20
#define IRQ_SPORT0_TX_POS	24
#define IRQ_SPORT1_RX_POS	28

#define IRQ_SPORT1_TX_POS	0
#define IRQ_TWI_POS		4
#define IRQ_SPI_POS		8
#define IRQ_UART0_RX_POS	12
#define IRQ_UART0_TX_POS	16
#define IRQ_UART1_RX_POS	20
#define IRQ_UART1_TX_POS	24
#define IRQ_CAN_RX_POS		28

#define IRQ_CAN_TX_POS		0
#define IRQ_MAC_RX_POS		4
#define IRQ_MAC_TX_POS		8
#define IRQ_TIMER0_POS		12
#define IRQ_TIMER1_POS		16
#define IRQ_TIMER2_POS		20
#define IRQ_TIMER3_POS		24
#define IRQ_TIMER4_POS		28

#define IRQ_TIMER5_POS		0
#define IRQ_TIMER6_POS		4
#define IRQ_TIMER7_POS		8
#define IRQ_PROG_INTA_POS	12
#define IRQ_PORTG_INTB_POS	16
#define IRQ_MEM_DMA0_POS	20
#define IRQ_MEM_DMA1_POS	24
#define IRQ_WATCH_POS		28

#define init_mach_irq init_mach_irq

#endif
