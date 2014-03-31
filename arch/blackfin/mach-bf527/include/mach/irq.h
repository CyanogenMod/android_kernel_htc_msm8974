/*
 * Copyright 2007-2008 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later
 */

#ifndef _BF527_IRQ_H_
#define _BF527_IRQ_H_

#include <mach-common/irq.h>

#define NR_PERI_INTS		(2 * 32)

#define IRQ_PLL_WAKEUP		BFIN_IRQ(0)	
#define IRQ_DMA0_ERROR		BFIN_IRQ(1)	
#define IRQ_DMAR0_BLK		BFIN_IRQ(2)	
#define IRQ_DMAR1_BLK		BFIN_IRQ(3)	
#define IRQ_DMAR0_OVR		BFIN_IRQ(4)	
#define IRQ_DMAR1_OVR		BFIN_IRQ(5)	
#define IRQ_PPI_ERROR		BFIN_IRQ(6)	
#define IRQ_MAC_ERROR		BFIN_IRQ(7)	
#define IRQ_SPORT0_ERROR	BFIN_IRQ(8)	
#define IRQ_SPORT1_ERROR	BFIN_IRQ(9)	
#define IRQ_UART0_ERROR		BFIN_IRQ(12)	
#define IRQ_UART1_ERROR		BFIN_IRQ(13)	
#define IRQ_RTC			BFIN_IRQ(14)	
#define IRQ_PPI			BFIN_IRQ(15)	
#define IRQ_SPORT0_RX		BFIN_IRQ(16)	
#define IRQ_SPORT0_TX		BFIN_IRQ(17)	
#define IRQ_SPORT1_RX		BFIN_IRQ(18)	
#define IRQ_SPORT1_TX		BFIN_IRQ(19)	
#define IRQ_TWI			BFIN_IRQ(20)	
#define IRQ_SPI			BFIN_IRQ(21)	
#define IRQ_UART0_RX		BFIN_IRQ(22)	
#define IRQ_UART0_TX		BFIN_IRQ(23)	
#define IRQ_UART1_RX		BFIN_IRQ(24)	
#define IRQ_UART1_TX		BFIN_IRQ(25)	
#define IRQ_OPTSEC		BFIN_IRQ(26)	
#define IRQ_CNT			BFIN_IRQ(27)	
#define IRQ_MAC_RX		BFIN_IRQ(28)	
#define IRQ_PORTH_INTA		BFIN_IRQ(29)	
#define IRQ_MAC_TX		BFIN_IRQ(30)	
#define IRQ_NFC			BFIN_IRQ(30)	
#define IRQ_PORTH_INTB		BFIN_IRQ(31)	
#define IRQ_TIMER0		BFIN_IRQ(32)	
#define IRQ_TIMER1		BFIN_IRQ(33)	
#define IRQ_TIMER2		BFIN_IRQ(34)	
#define IRQ_TIMER3		BFIN_IRQ(35)	
#define IRQ_TIMER4		BFIN_IRQ(36)	
#define IRQ_TIMER5		BFIN_IRQ(37)	
#define IRQ_TIMER6		BFIN_IRQ(38)	
#define IRQ_TIMER7		BFIN_IRQ(39)	
#define IRQ_PORTG_INTA		BFIN_IRQ(40)	
#define IRQ_PORTG_INTB		BFIN_IRQ(41)	
#define IRQ_MEM_DMA0		BFIN_IRQ(42)	
#define IRQ_MEM_DMA1		BFIN_IRQ(43)	
#define IRQ_WATCH		BFIN_IRQ(44)	
#define IRQ_PORTF_INTA		BFIN_IRQ(45)	
#define IRQ_PORTF_INTB		BFIN_IRQ(46)	
#define IRQ_SPI_ERROR		BFIN_IRQ(47)	
#define IRQ_NFC_ERROR		BFIN_IRQ(48)	
#define IRQ_HDMA_ERROR		BFIN_IRQ(49)	
#define IRQ_HDMA		BFIN_IRQ(50)	
#define IRQ_USB_EINT		BFIN_IRQ(51)	
#define IRQ_USB_INT0		BFIN_IRQ(52)	
#define IRQ_USB_INT1		BFIN_IRQ(53)	
#define IRQ_USB_INT2		BFIN_IRQ(54)	
#define IRQ_USB_DMA		BFIN_IRQ(55)	

#define SYS_IRQS		BFIN_IRQ(63)	

#define IRQ_PF0			71
#define IRQ_PF1			72
#define IRQ_PF2			73
#define IRQ_PF3			74
#define IRQ_PF4			75
#define IRQ_PF5			76
#define IRQ_PF6			77
#define IRQ_PF7			78
#define IRQ_PF8			79
#define IRQ_PF9			80
#define IRQ_PF10		81
#define IRQ_PF11		82
#define IRQ_PF12		83
#define IRQ_PF13		84
#define IRQ_PF14		85
#define IRQ_PF15		86

#define IRQ_PG0			87
#define IRQ_PG1			88
#define IRQ_PG2			89
#define IRQ_PG3			90
#define IRQ_PG4			91
#define IRQ_PG5			92
#define IRQ_PG6			93
#define IRQ_PG7			94
#define IRQ_PG8			95
#define IRQ_PG9			96
#define IRQ_PG10		97
#define IRQ_PG11		98
#define IRQ_PG12		99
#define IRQ_PG13		100
#define IRQ_PG14		101
#define IRQ_PG15		102

#define IRQ_PH0			103
#define IRQ_PH1			104
#define IRQ_PH2			105
#define IRQ_PH3			106
#define IRQ_PH4			107
#define IRQ_PH5			108
#define IRQ_PH6			109
#define IRQ_PH7			110
#define IRQ_PH8			111
#define IRQ_PH9			112
#define IRQ_PH10		113
#define IRQ_PH11		114
#define IRQ_PH12		115
#define IRQ_PH13		116
#define IRQ_PH14		117
#define IRQ_PH15		118

#define GPIO_IRQ_BASE		IRQ_PF0

#define IRQ_MAC_PHYINT		119	
#define IRQ_MAC_MMCINT		120	
#define IRQ_MAC_RXFSINT		121	
#define IRQ_MAC_TXFSINT		122	
#define IRQ_MAC_WAKEDET		123	
#define IRQ_MAC_RXDMAERR	124	
#define IRQ_MAC_TXDMAERR	125	
#define IRQ_MAC_STMDONE		126	

#define NR_MACH_IRQS		(IRQ_MAC_STMDONE + 1)

#define IRQ_PLL_WAKEUP_POS	0
#define IRQ_DMA0_ERROR_POS	4
#define IRQ_DMAR0_BLK_POS	8
#define IRQ_DMAR1_BLK_POS	12
#define IRQ_DMAR0_OVR_POS	16
#define IRQ_DMAR1_OVR_POS	20
#define IRQ_PPI_ERROR_POS	24
#define IRQ_MAC_ERROR_POS	28

#define IRQ_SPORT0_ERROR_POS	0
#define IRQ_SPORT1_ERROR_POS	4
#define IRQ_UART0_ERROR_POS	16
#define IRQ_UART1_ERROR_POS	20
#define IRQ_RTC_POS		24
#define IRQ_PPI_POS		28

#define IRQ_SPORT0_RX_POS	0
#define IRQ_SPORT0_TX_POS	4
#define IRQ_SPORT1_RX_POS	8
#define IRQ_SPORT1_TX_POS	12
#define IRQ_TWI_POS		16
#define IRQ_SPI_POS		20
#define IRQ_UART0_RX_POS	24
#define IRQ_UART0_TX_POS	28

#define IRQ_UART1_RX_POS	0
#define IRQ_UART1_TX_POS	4
#define IRQ_OPTSEC_POS		8
#define IRQ_CNT_POS		12
#define IRQ_MAC_RX_POS		16
#define IRQ_PORTH_INTA_POS	20
#define IRQ_MAC_TX_POS		24
#define IRQ_PORTH_INTB_POS	28

#define IRQ_TIMER0_POS		0
#define IRQ_TIMER1_POS		4
#define IRQ_TIMER2_POS		8
#define IRQ_TIMER3_POS		12
#define IRQ_TIMER4_POS		16
#define IRQ_TIMER5_POS		20
#define IRQ_TIMER6_POS		24
#define IRQ_TIMER7_POS		28

#define IRQ_PORTG_INTA_POS	0
#define IRQ_PORTG_INTB_POS	4
#define IRQ_MEM_DMA0_POS	8
#define IRQ_MEM_DMA1_POS	12
#define IRQ_WATCH_POS		16
#define IRQ_PORTF_INTA_POS	20
#define IRQ_PORTF_INTB_POS	24
#define IRQ_SPI_ERROR_POS	28

#define IRQ_NFC_ERROR_POS	0
#define IRQ_HDMA_ERROR_POS	4
#define IRQ_HDMA_POS		8
#define IRQ_USB_EINT_POS	12
#define IRQ_USB_INT0_POS	16
#define IRQ_USB_INT1_POS	20
#define IRQ_USB_INT2_POS	24
#define IRQ_USB_DMA_POS		28

#endif
