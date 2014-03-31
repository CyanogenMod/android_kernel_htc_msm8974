/*
 * arch/arm/mach-sa1100/include/mach/irqs.h
 *
 * Copyright (C) 1996 Russell King
 * Copyright (C) 1998 Deborah Wallach (updates for SA1100/Brutus).
 * Copyright (C) 1999 Nicolas Pitre (full GPIO irq isolation)
 *
 * 2001/11/14	RMK	Cleaned up and standardised a lot of the IRQs.
 */

#define	IRQ_GPIO0		0
#define	IRQ_GPIO1		1
#define	IRQ_GPIO2		2
#define	IRQ_GPIO3		3
#define	IRQ_GPIO4		4
#define	IRQ_GPIO5		5
#define	IRQ_GPIO6		6
#define	IRQ_GPIO7		7
#define	IRQ_GPIO8		8
#define	IRQ_GPIO9		9
#define	IRQ_GPIO10		10
#define	IRQ_GPIO11_27		11
#define	IRQ_LCD  		12	
#define	IRQ_Ser0UDC		13	
#define	IRQ_Ser1SDLC		14	
#define	IRQ_Ser1UART		15	
#define	IRQ_Ser2ICP		16	
#define	IRQ_Ser3UART		17	
#define	IRQ_Ser4MCP		18	
#define	IRQ_Ser4SSP		19	
#define	IRQ_DMA0 		20	
#define	IRQ_DMA1 		21	
#define	IRQ_DMA2 		22	
#define	IRQ_DMA3 		23	
#define	IRQ_DMA4 		24	
#define	IRQ_DMA5 		25	
#define	IRQ_OST0 		26	
#define	IRQ_OST1 		27	
#define	IRQ_OST2 		28	
#define	IRQ_OST3 		29	
#define	IRQ_RTC1Hz		30	
#define	IRQ_RTCAlrm		31	

#define	IRQ_GPIO11		32
#define	IRQ_GPIO12		33
#define	IRQ_GPIO13		34
#define	IRQ_GPIO14		35
#define	IRQ_GPIO15		36
#define	IRQ_GPIO16		37
#define	IRQ_GPIO17		38
#define	IRQ_GPIO18		39
#define	IRQ_GPIO19		40
#define	IRQ_GPIO20		41
#define	IRQ_GPIO21		42
#define	IRQ_GPIO22		43
#define	IRQ_GPIO23		44
#define	IRQ_GPIO24		45
#define	IRQ_GPIO25		46
#define	IRQ_GPIO26		47
#define	IRQ_GPIO27		48

#define IRQ_BOARD_START		49
#define IRQ_BOARD_END		65

#ifdef CONFIG_SHARP_LOCOMO
#define NR_IRQS_LOCOMO		4
#else
#define NR_IRQS_LOCOMO		0
#endif

#ifndef NR_IRQS
#define NR_IRQS (IRQ_BOARD_START + NR_IRQS_LOCOMO)
#endif
#define SA1100_NR_IRQS (IRQ_BOARD_START + NR_IRQS_LOCOMO)
