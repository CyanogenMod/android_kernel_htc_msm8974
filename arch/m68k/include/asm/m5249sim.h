
/*
 *	m5249sim.h -- ColdFire 5249 System Integration Module support.
 *
 *	(C) Copyright 2002, Greg Ungerer (gerg@snapgear.com)
 */

#ifndef	m5249sim_h
#define	m5249sim_h

#define	CPU_NAME		"COLDFIRE(m5249)"
#define	CPU_INSTR_PER_JIFFY	3
#define	MCF_BUSCLK		(MCF_CLK / 2)

#include <asm/m52xxacr.h>

#define MCF_MBAR2		0x80000000

#define	MCFSIM_RSR		0x00		
#define	MCFSIM_SYPCR		0x01		
#define	MCFSIM_SWIVR		0x02		
#define	MCFSIM_SWSR		0x03		
#define	MCFSIM_PAR		0x04		
#define	MCFSIM_IRQPAR		0x06		
#define	MCFSIM_MPARK		0x0C		
#define	MCFSIM_IPR		0x40		
#define	MCFSIM_IMR		0x44		
#define	MCFSIM_AVR		0x4b		
#define	MCFSIM_ICR0		0x4c		
#define	MCFSIM_ICR1		0x4d		
#define	MCFSIM_ICR2		0x4e		
#define	MCFSIM_ICR3		0x4f		
#define	MCFSIM_ICR4		0x50		
#define	MCFSIM_ICR5		0x51		
#define	MCFSIM_ICR6		0x52		
#define	MCFSIM_ICR7		0x53		
#define	MCFSIM_ICR8		0x54		
#define	MCFSIM_ICR9		0x55		
#define	MCFSIM_ICR10		0x56		
#define	MCFSIM_ICR11		0x57		

#define MCFSIM_CSAR0		0x80		
#define MCFSIM_CSMR0		0x84		
#define MCFSIM_CSCR0		0x8a		
#define MCFSIM_CSAR1		0x8c		
#define MCFSIM_CSMR1		0x90		
#define MCFSIM_CSCR1		0x96		
#define MCFSIM_CSAR2		0x98		
#define MCFSIM_CSMR2		0x9c		
#define MCFSIM_CSCR2		0xa2		
#define MCFSIM_CSAR3		0xa4		
#define MCFSIM_CSMR3		0xa8		
#define MCFSIM_CSCR3		0xae		

#define MCFSIM_DCR		(MCF_MBAR + 0x100)	
#define MCFSIM_DACR0		(MCF_MBAR + 0x108)	
#define MCFSIM_DMR0		(MCF_MBAR + 0x10c)	
#define MCFSIM_DACR1		(MCF_MBAR + 0x110)	
#define MCFSIM_DMR1		(MCF_MBAR + 0x114)	

#define MCFTIMER_BASE1		(MCF_MBAR + 0x140)	
#define MCFTIMER_BASE2		(MCF_MBAR + 0x180)	

#define MCFUART_BASE0		(MCF_MBAR + 0x1c0)	
#define MCFUART_BASE1		(MCF_MBAR + 0x200)	

#define	MCFQSPI_BASE		(MCF_MBAR + 0x300)	
#define	MCFQSPI_SIZE		0x40			

#define	MCFQSPI_CS0		29
#define	MCFQSPI_CS1		24
#define	MCFQSPI_CS2		21
#define	MCFQSPI_CS3		22

#define MCFDMA_BASE0		(MCF_MBAR + 0x300)	
#define MCFDMA_BASE1		(MCF_MBAR + 0x340)	
#define MCFDMA_BASE2		(MCF_MBAR + 0x380)	
#define MCFDMA_BASE3		(MCF_MBAR + 0x3C0)	

#define	MCFSIM_SWDICR		MCFSIM_ICR0	
#define	MCFSIM_TIMER1ICR	MCFSIM_ICR1	
#define	MCFSIM_TIMER2ICR	MCFSIM_ICR2	
#define	MCFSIM_UART1ICR		MCFSIM_ICR4	
#define	MCFSIM_UART2ICR		MCFSIM_ICR5	
#define	MCFSIM_DMA0ICR		MCFSIM_ICR6	
#define	MCFSIM_DMA1ICR		MCFSIM_ICR7	
#define	MCFSIM_DMA2ICR		MCFSIM_ICR8	
#define	MCFSIM_DMA3ICR		MCFSIM_ICR9	
#define	MCFSIM_QSPIICR		MCFSIM_ICR10	

#define	MCF_IRQ_QSPI		28		
#define	MCF_IRQ_TIMER		30		
#define	MCF_IRQ_PROFILER	31		

#define	MCF_IRQ_UART0		73		
#define	MCF_IRQ_UART1		74		

#define	MCFSIM2_GPIOREAD	(MCF_MBAR2 + 0x000)	
#define	MCFSIM2_GPIOWRITE	(MCF_MBAR2 + 0x004)	
#define	MCFSIM2_GPIOENABLE	(MCF_MBAR2 + 0x008)	
#define	MCFSIM2_GPIOFUNC	(MCF_MBAR2 + 0x00C)	
#define	MCFSIM2_GPIO1READ	(MCF_MBAR2 + 0x0B0)	
#define	MCFSIM2_GPIO1WRITE	(MCF_MBAR2 + 0x0B4)	
#define	MCFSIM2_GPIO1ENABLE	(MCF_MBAR2 + 0x0B8)	
#define	MCFSIM2_GPIO1FUNC	(MCF_MBAR2 + 0x0BC)	

#define	MCFSIM2_GPIOINTSTAT	0xc0		
#define	MCFSIM2_GPIOINTCLEAR	0xc0		
#define	MCFSIM2_GPIOINTENABLE	0xc4		

#define	MCFSIM2_INTLEVEL1	0x140		
#define	MCFSIM2_INTLEVEL2	0x144		
#define	MCFSIM2_INTLEVEL3	0x148		
#define	MCFSIM2_INTLEVEL4	0x14c		
#define	MCFSIM2_INTLEVEL5	0x150		
#define	MCFSIM2_INTLEVEL6	0x154		
#define	MCFSIM2_INTLEVEL7	0x158		
#define	MCFSIM2_INTLEVEL8	0x15c		

#define	MCFSIM2_DMAROUTE	0x188		

#define	MCFSIM2_IDECONFIG1	0x18c		
#define	MCFSIM2_IDECONFIG2	0x190		

#define	MCFINTC2_VECBASE	128

#define	MCFINTC2_GPIOIRQ0	(MCFINTC2_VECBASE + 32)
#define	MCFINTC2_GPIOIRQ1	(MCFINTC2_VECBASE + 33)
#define	MCFINTC2_GPIOIRQ2	(MCFINTC2_VECBASE + 34)
#define	MCFINTC2_GPIOIRQ3	(MCFINTC2_VECBASE + 35)
#define	MCFINTC2_GPIOIRQ4	(MCFINTC2_VECBASE + 36)
#define	MCFINTC2_GPIOIRQ5	(MCFINTC2_VECBASE + 37)
#define	MCFINTC2_GPIOIRQ6	(MCFINTC2_VECBASE + 38)
#define	MCFINTC2_GPIOIRQ7	(MCFINTC2_VECBASE + 39)

#define MCFGPIO_PIN_MAX		64
#define MCFGPIO_IRQ_MAX		-1
#define MCFGPIO_IRQ_VECBASE	-1


#ifdef __ASSEMBLER__

.macro m5249c3_setup
	movel	#0x10000001,%a0
	movec	%a0,%MBAR			
	subql	#1,%a0				

	movel	#0x80000001,%a1
	movec	%a1,#3086			
	subql	#1,%a1				

	moveb	#MCFINTC2_VECBASE,%d0
	moveb	%d0,0x16b(%a1)			

	movel	#0x001F0021,%d0			
	movel	%d0,0x84(%a0)			

	movel	0x180(%a1),%d0			
	andl	#0xfffffffe,%d0			
	movel	%d0,0x180(%a1)			
	nop

#if CONFIG_CLOCK_FREQ == 140000000
	movel	#0x125a40f0,%d0			
	movel	%d0,0x180(%a1)			
	orl	#0x1,%d0
	movel	%d0,0x180(%a1)			
#endif

	movel  #0xe0000000,%d0			
	movel  %d0,0x8c(%a0)
	movel  #0x001f0021,%d0			
	movel  %d0,0x90(%a0)
	movew  #0x0080,%d0			
	movew  %d0,0x96(%a0)

	movel	#0x50000000,%d0			
	movel	%d0,0x98(%a0)
	movel	#0x001f0001,%d0			
	movel	%d0,0x9c(%a0)
	movew	#0x0080,%d0			
	movew	%d0,0xa2(%a0)

	movel	#0x00107000,%d0			
	movel	%d0,0x18c(%a1)
	movel	#0x000c0400,%d0			
	movel	%d0,0x190(%a1)

	movel	#0x00080000,%d0			
	orl	%d0,0xc(%a1)			
	orl	%d0,0x8(%a1)			
        orl	%d0,0x4(%a1)			
.endm

#define	PLATFORM_SETUP	m5249c3_setup

#endif 

#endif	
