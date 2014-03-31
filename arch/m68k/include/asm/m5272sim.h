
/*
 *	m5272sim.h -- ColdFire 5272 System Integration Module support.
 *
 *	(C) Copyright 1999, Greg Ungerer (gerg@snapgear.com)
 * 	(C) Copyright 2000, Lineo Inc. (www.lineo.com) 
 */

#ifndef	m5272sim_h
#define	m5272sim_h

#define	CPU_NAME		"COLDFIRE(m5272)"
#define	CPU_INSTR_PER_JIFFY	3
#define	MCF_BUSCLK		MCF_CLK

#include <asm/m52xxacr.h>

#define	MCFSIM_SCR		0x04		
#define	MCFSIM_SPR		0x06		
#define	MCFSIM_PMR		0x08		
#define	MCFSIM_APMR		0x0e		
#define	MCFSIM_DIR		0x10		

#define	MCFSIM_ICR1		0x20		
#define	MCFSIM_ICR2		0x24		
#define	MCFSIM_ICR3		0x28		
#define	MCFSIM_ICR4		0x2c		

#define MCFSIM_ISR		0x30		
#define MCFSIM_PITR		0x34		
#define	MCFSIM_PIWR		0x38		
#define	MCFSIM_PIVR		0x3f		

#define	MCFSIM_WRRR		0x280		
#define	MCFSIM_WIRR		0x284		
#define	MCFSIM_WCR		0x288		
#define	MCFSIM_WER		0x28c		

#define	MCFSIM_CSBR0		0x40		
#define	MCFSIM_CSOR0		0x44		
#define	MCFSIM_CSBR1		0x48		
#define	MCFSIM_CSOR1		0x4c		
#define	MCFSIM_CSBR2		0x50		
#define	MCFSIM_CSOR2		0x54		
#define	MCFSIM_CSBR3		0x58		
#define	MCFSIM_CSOR3		0x5c		
#define	MCFSIM_CSBR4		0x60		
#define	MCFSIM_CSOR4		0x64		
#define	MCFSIM_CSBR5		0x68		
#define	MCFSIM_CSOR5		0x6c		
#define	MCFSIM_CSBR6		0x70		
#define	MCFSIM_CSOR6		0x74		
#define	MCFSIM_CSBR7		0x78		
#define	MCFSIM_CSOR7		0x7c		

#define	MCFSIM_SDCR		0x180		
#define	MCFSIM_SDTR		0x184		
#define	MCFSIM_DCAR0		0x4c		
#define	MCFSIM_DCMR0		0x50		
#define	MCFSIM_DCCR0		0x57		
#define	MCFSIM_DCAR1		0x58		
#define	MCFSIM_DCMR1		0x5c		
#define	MCFSIM_DCCR1		0x63		

#define	MCFUART_BASE0		(MCF_MBAR + 0x100) 
#define	MCFUART_BASE1		(MCF_MBAR + 0x140) 

#define	MCFSIM_PACNT		(MCF_MBAR + 0x80) 
#define	MCFSIM_PADDR		(MCF_MBAR + 0x84) 
#define	MCFSIM_PADAT		(MCF_MBAR + 0x86) 
#define	MCFSIM_PBCNT		(MCF_MBAR + 0x88) 
#define	MCFSIM_PBDDR		(MCF_MBAR + 0x8c) 
#define	MCFSIM_PBDAT		(MCF_MBAR + 0x8e) 
#define	MCFSIM_PCDDR		(MCF_MBAR + 0x94) 
#define	MCFSIM_PCDAT		(MCF_MBAR + 0x96) 
#define	MCFSIM_PDCNT		(MCF_MBAR + 0x98) 

#define	MCFDMA_BASE0		(MCF_MBAR + 0xe0) 

#define	MCFTIMER_BASE1		(MCF_MBAR + 0x200) 
#define	MCFTIMER_BASE2		(MCF_MBAR + 0x220) 
#define	MCFTIMER_BASE3		(MCF_MBAR + 0x240) 
#define	MCFTIMER_BASE4		(MCF_MBAR + 0x260) 

#define	MCFFEC_BASE0		(MCF_MBAR + 0x840) 
#define	MCFFEC_SIZE0		0x1d0

#define	MCFINT_VECBASE		64		
#define	MCF_IRQ_SPURIOUS	64		
#define	MCF_IRQ_EINT1		65		
#define	MCF_IRQ_EINT2		66		
#define	MCF_IRQ_EINT3		67		
#define	MCF_IRQ_EINT4		68		
#define	MCF_IRQ_TIMER1		69		
#define	MCF_IRQ_TIMER2		70		
#define	MCF_IRQ_TIMER3		71		
#define	MCF_IRQ_TIMER4		72		
#define	MCF_IRQ_UART0		73		
#define	MCF_IRQ_UART1		74		
#define	MCF_IRQ_PLIP		75		
#define	MCF_IRQ_PLIA		76		
#define	MCF_IRQ_USB0		77		
#define	MCF_IRQ_USB1		78		
#define	MCF_IRQ_USB2		79		
#define	MCF_IRQ_USB3		80		
#define	MCF_IRQ_USB4		81		
#define	MCF_IRQ_USB5		82		
#define	MCF_IRQ_USB6		83		
#define	MCF_IRQ_USB7		84		
#define	MCF_IRQ_DMA		85		
#define	MCF_IRQ_FECRX0		86		
#define	MCF_IRQ_FECTX0		87		
#define	MCF_IRQ_FECENTC0	88		
#define	MCF_IRQ_QSPI		89		
#define	MCF_IRQ_EINT5		90		
#define	MCF_IRQ_EINT6		91		
#define	MCF_IRQ_SWTO		92		
#define	MCFINT_VECMAX		95		

#define	MCF_IRQ_TIMER		MCF_IRQ_TIMER1
#define	MCF_IRQ_PROFILER	MCF_IRQ_TIMER2

#define MCFGPIO_PIN_MAX			48
#define MCFGPIO_IRQ_MAX			-1
#define MCFGPIO_IRQ_VECBASE		-1
#endif	
